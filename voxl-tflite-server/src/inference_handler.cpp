#include "inference_handler.h"
#include <chrono>
#include <atomic>

// Global variables to track frames and timing
static std::atomic<int> frames_processed{0};
static std::chrono::time_point<std::chrono::high_resolution_clock> pipeline_start_time;

// the preprocess step is around 3.2x faster (as tested on yolov8)
// and therefore the queue is only be of about length 4
static std::queue<std::shared_ptr<PipelineData>> preprocess_inference_queue; // queue to handle the output of the preprocess step
static std::queue<std::shared_ptr<PipelineData>> inference_postprocess_queue;

static std::mutex preprocess_inference_mutex, inference_postprocess_mutex, postprocess_mutex;
static std::condition_variable preprocess_inference_cond, inference_postprocess_cond, postprocess_cond;
bool postprocess_finish = false;

static inline void set_core_affinity()
{
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(6, &cpuset); // High-performance core 6
    CPU_SET(5, &cpuset); // High-performance core 5
    CPU_SET(4, &cpuset); // High-performance core 4
    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset))
    {
        perror("pthread_setaffinity_np");
    }

    /* Check the actual affinity mask assigned to the thread */
    if (pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset))
    {
        perror("pthread_getaffinity_np");
    }
    for (int j = 0; j < CPU_SETSIZE; j++)
    {
        if (CPU_ISSET(j, &cpuset))
            printf(" %d", j);
    }
    printf("\n");
}


void *run_inference_pipeline(void *args)
{
    InferenceWorkerArgs *worker_args = static_cast<InferenceWorkerArgs *>(args);

    pipeline_start_time = std::chrono::high_resolution_clock::now();

    std::thread preprocess_thread(preprocess_worker, worker_args->model_helper);
    std::thread inference_thread(inference_worker, worker_args->model_helper);
    std::thread postprocess_thread(postprocess_worker, worker_args->model_helper);

    // Wait for threads to finish or handle cleanup
    preprocess_thread.join();
    inference_thread.join();
    postprocess_thread.join();

    return nullptr;

    /*
    This worker recieves the frame from the camera pipe and runs the
    preprocess step on it. Once that is done it populates the preprocessed frame into
    the inference queue for the inference_postprocess worker to deal with
    */
}
void preprocess_worker(ModelHelper *model_helper)
{

#ifdef BUILD_QRB5165
    set_core_affinity();
#endif

    int queue_index = 0;

    while (main_running)
    {
        // this is the only place where the camera queue data is extracted
        // the queue is populated by the camera helper function meanwhile
        if (queue_index == model_helper->camera_queue.insert_idx)
        {
            std::unique_lock<std::mutex> lock(model_helper->cond_mutex);
            model_helper->cond_var.wait(lock);
            continue;
        }
        
        // grab the frame and bump our queue index, making sure its within queue
        // size
        TFLiteMessage *new_frame = &model_helper->camera_queue.queue[queue_index];
        queue_index = ((queue_index + 1) % QUEUE_SIZE);


        if (!main_running) {
            break; // Exit if main loop has stopped
        }

        auto output_image = std::make_shared<cv::Mat>();

        // points to the same resize_output memory buffer created in the 
        // preprocess method
        auto preprocessed_image = std::make_shared<cv::Mat>();

        if (!model_helper->preprocess(new_frame->metadata, (char *)new_frame->image_pixels, preprocessed_image, output_image)) {
            continue;
        }
        else
        {
            std::lock_guard<std::mutex> inference_lock(preprocess_inference_mutex);

                    auto pipeline_data = std::make_shared<PipelineData>();
        pipeline_data->preprocessed_image = preprocessed_image;
        pipeline_data->metadata = new_frame->metadata;
        pipeline_data->output_image = output_image;
        // last_inferenence_time is not initialized for now

        preprocess_inference_queue.push(pipeline_data);
        preprocess_inference_cond.notify_one();
        }
    }
}

void inference_worker(ModelHelper *model_helper)
{

    while (main_running)
    {
        postprocess_finish = false;
        std::unique_lock<std::mutex> lock(preprocess_inference_mutex);
        preprocess_inference_cond.wait(lock, []
                                       { return !preprocess_inference_queue.empty() || !main_running; });
        if (!main_running) {
            break;
        }

        std::shared_ptr<PipelineData> pipeline_data;
        pipeline_data = preprocess_inference_queue.front();
        preprocess_inference_queue.pop();

        lock.unlock();

        double last_inference_time = 0;

        if (!model_helper->run_inference(*pipeline_data->preprocessed_image, &last_inference_time)) {
            continue;
        }
        else
        {
            pipeline_data->last_inference_time = last_inference_time;
            std::lock_guard<std::mutex> inference_postprocess_lock(inference_postprocess_mutex);
            inference_postprocess_queue.push(pipeline_data);
            inference_postprocess_cond.notify_one();
        }

        while (preprocess_inference_queue.size() > QUEUE_LIMIT) {
            preprocess_inference_queue.pop();
        }

        while (inference_postprocess_queue.size() > QUEUE_LIMIT) {
            inference_postprocess_queue.pop();
        }

        std::unique_lock<std::mutex> lock_postprocess(postprocess_mutex);
        postprocess_cond.wait(lock_postprocess, []
                                        { return postprocess_finish; });
    }
}

void postprocess_worker(ModelHelper *model_helper)
{
#ifdef BUILD_QRB5165
    set_core_affinity();
#endif

    while (main_running)
    {
        std::unique_lock<std::mutex> lock(inference_postprocess_mutex);
        inference_postprocess_cond.wait(lock, []
                                        { return !inference_postprocess_queue.empty() || !main_running; });
        if (!main_running) {
            break;
        }

        std::shared_ptr<PipelineData> pipeline_data;
        pipeline_data = inference_postprocess_queue.front();
        inference_postprocess_queue.pop();

        lock.unlock();

        // set this field here to allow the deep lab post processer to use it
        model_helper->preprocessed_image = pipeline_data->preprocessed_image;
        std::shared_ptr<cv::Mat> output_image = pipeline_data->output_image;
        // sets up post processing and related operations
        if (!model_helper->worker(*output_image, pipeline_data->last_inference_time, pipeline_data->metadata, pipeline_data.get())) {
            postprocess_finish = true;
            postprocess_cond.notify_one();
            continue;
        }
        else
        {
            frames_processed++;
            if (frames_processed % 10 == 0)
            {
                auto now = std::chrono::high_resolution_clock::now();
                double elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - pipeline_start_time).count();
                double throughput = frames_processed / elapsed_seconds; // frames per second
                std::cout << "Current pipeline throughput: " << throughput << " frames per second" << std::endl;
            }
        }
        postprocess_finish = true;
        postprocess_cond.notify_one();
    }
}
