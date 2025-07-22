#include "config_file.h"
#include "utils.h"
#include "model_helper/model_info.h"
#include "inference_handler.h"

#define PROCESS_NAME "voxl-tflite-server"
#define HIRES_PIPE "/run/mpa/hires_small_color/"
#define TFLITE_IMAGE_PATH (MODAL_PIPE_DEFAULT_BASE_DIR "tflite/")
#define TFLITE_DETECTION_PATH (MODAL_PIPE_DEFAULT_BASE_DIR "tflite_data/")

ModelHelper *model_helper;

bool en_debug = false;
bool en_timing = false;
char *coco_labels = (char *)"/usr/bin/dnn/coco_labels.txt";
char *city_labels = (char *)"/usr/bin/dnn/cityscapes_labels.txt";
char *imagenet_labels = (char *)"/usr/bin/dnn/imagenet_labels.txt";
char *yolo_labels = (char *)"/usr/bin/dnn/yolov5_labels.txt";
// char *gate_xyz_labels = (char*)"/usr/bin/dnn/gate_xyz_labels.txt";
// char *gate_yaw_labels  = (char*)"/usr/bin/dnn/gate_yaw_labels.txt";
// yolov8 has the same labels for the coco dataset

static void _camera_helper_cb(__attribute__((unused)) int ch,
                              camera_image_metadata_t meta, char *frame,
                              void *context);
static void _camera_disconnect_cb(__attribute__((unused)) int ch,
                                  __attribute__((unused)) void *context);
static void _camera_connect_cb(__attribute__((unused)) int ch,
                               __attribute__((unused)) void *context);
static void set_delegate(DelegateOpt *opt);
static void initialize_model_settings(char *model, char *delegate, ModelName *model_name, ModelCategory *model_category, NormalizationType *norm_type);

int main(int argc, char *argv[])
{
    if (_parse_opts(argc, argv, &en_debug, &en_timing))
        return -1;

    ////////////////////////////////////////////////////////////////////////////////
    // load config, need to do before checking for other instances
    ////////////////////////////////////////////////////////////////////////////////
    if (config_file_read())
        return -1;

    config_file_print();

    if (!allow_multiple)
    {
        ////////////////////////////////////////////////////////////////////////////////
        // gracefully handle an existing instance of the process and associated
        // PID file
        ////////////////////////////////////////////////////////////////////////////////

        // make sure another instance isn't running
        // if return value is -3 then a background process is running with
        // higher privaledges and we couldn't kill it, in which case we should
        // not continue or there may be hardware conflicts. If it returned -4
        // then there was an invalid argument that needs to be fixed.
        if (kill_existing_process(PROCESS_NAME, 2.0) < -2)
            return -1;

        // start signal handler so we can exit cleanly
        if (enable_signal_handler() == -1)
        {
            fprintf(stderr, "ERROR: failed to start signal handler\n");
            return (-1);
        }

        // make PID file to indicate your project is running
        // due to the check made on the call to rc_kill_existing_process() above
        // we can be fairly confident there is no PID file already and we can
        // make our own safely.
        make_pid_file(PROCESS_NAME);
    }

    // disable garbage multithreading
    cv::setNumThreads(1);

    ModelName model_name;
    ModelCategory model_category;
    DelegateOpt opt_;
    NormalizationType do_normalize = NONE;

    ////////////////////////////////////////////////////////////////////////////////
    // initialize InferenceHelper
    ////////////////////////////////////////////////////////////////////////////////

    set_delegate(&opt_);
    initialize_model_settings(model, delegate, &model_name, &model_category, &do_normalize);

    model_helper = create_model_helper(model_name, model_category, opt_, do_normalize);

    // store cam name
    std::string full_path(input_pipe);
    std::string cam_name(
        full_path.substr(full_path.rfind("/", full_path.size() - 2) + 1));
    cam_name.pop_back();

    model_helper->cam_name = cam_name;

    main_running = 1;

    fprintf(stderr, "\n------VOXL TFLite Server------\n\n");

    // Start the thread that will run the tensorflow lite model on live camera
    // frames.
    pthread_attr_t thread_attributes;
    pthread_attr_init(&thread_attributes);
    pthread_attr_setdetachstate(&thread_attributes, PTHREAD_CREATE_JOINABLE);

    InferenceWorkerArgs *args = new InferenceWorkerArgs;
    args->model_helper = model_helper;

    int ret = pthread_create(&(model_helper->thread), &thread_attributes, run_inference_pipeline, args);
    if (ret != 0)
    {
        fprintf(stderr, "Error creating inference worker thread: %d\n", ret);
        delete args; // Clean up in case of failure
    }

    // fire up our camera server connection
    int ch = pipe_client_get_next_available_channel();

    pipe_client_set_connect_cb(ch, _camera_connect_cb, NULL);
    pipe_client_set_disconnect_cb(ch, _camera_disconnect_cb, NULL);
    pipe_client_set_camera_helper_cb(ch, _camera_helper_cb, NULL);

    if (pipe_client_open(ch, input_pipe, PROCESS_NAME,
                         CLIENT_FLAG_EN_CAMERA_HELPER, 0))
    {
        fprintf(stderr, "Failed to open pipe: %s\n", input_pipe);
        return -1;
    }

    if (!allow_multiple)
    {
        // open our output pipes using default names
        pipe_info_t image_pipe = {
            "tflite", TFLITE_IMAGE_PATH, "camera_image_metadata_t",
            PROCESS_NAME, 16 * 1024 * 1024, 0};
        pipe_server_create(IMAGE_CH, image_pipe, 0);
        if (model_category == OBJECT_DETECTION)
        {
            pipe_info_t detection_pipe = {
                "tflite_data", TFLITE_DETECTION_PATH,
                "ai_detection_t", PROCESS_NAME,
                16 * 1024, 0};
            pipe_server_create(DETECTION_CH, detection_pipe, 0);
        }
    }
    else
    {
        // set up a string to hold our custom pipe name
        std::string output_pipe_holder = MODAL_PIPE_DEFAULT_BASE_DIR;
        output_pipe_holder.append(output_pipe_prefix);
        output_pipe_holder.append("_tflite");

        // get c ptr to string
        const char *buf_ptr = output_pipe_holder.c_str();

        // create our output pipe
        pipe_info_t image_pipe = {
            "tflite", "unknown", "camera_image_metadata_t",
            PROCESS_NAME, 16 * 1024 * 1024, 0};
        // set the location to our new strings ptr
        memcpy(image_pipe.location, buf_ptr, MODAL_PIPE_MAX_NAME_LEN);
        // create the server pipe
        pipe_server_create(IMAGE_CH, image_pipe, 0);

        if (model_category == OBJECT_DETECTION)
        {
            // initialize the detection pipe only if we are running a detection
            // model
            pipe_info_t detection_pipe = {"tflite_data", "unknown",
                                          "ai_detection_t", PROCESS_NAME,
                                          16 * 1024, 0};
            output_pipe_holder = MODAL_PIPE_DEFAULT_BASE_DIR;
            output_pipe_holder.append(output_pipe_prefix);
            output_pipe_holder.append("_tflite_data");

            buf_ptr = output_pipe_holder.c_str();

            memcpy(detection_pipe.location, buf_ptr, MODAL_PIPE_MAX_NAME_LEN);

            pipe_server_create(DETECTION_CH, detection_pipe, 0);
        }
    }

    while (main_running)
    {
        usleep(5000000);
    }

    pipe_client_close_all();
    pipe_server_close_all();

    fprintf(stderr, "\nStopping the application\n");

    model_helper->cond_var.notify_all();
    pthread_join(model_helper->thread, NULL);

    delete (args);
    delete (model_helper);
    return 0;
}

// offset for classification models only (as of now), used for addressing same
// tensor data with varied offsets i.e. efficient net has 0 offset [0,1000],
// mobilenetv1 has +1 offset [1, 1001], etc...

static void _camera_connect_cb(__attribute__((unused)) int ch,
                               __attribute__((unused)) void *context)
{
    printf("Connected to camera server\n");
}

static void _camera_disconnect_cb(__attribute__((unused)) int ch,
                                  __attribute__((unused)) void *context)
{
    fprintf(stderr, "Disonnected from camera server\n");
}

static void _camera_helper_cb(__attribute__((unused)) int ch,
                              camera_image_metadata_t meta, char *frame,
                              void *context)
{
    static int n_skipped = 0;
    if (n_skipped < skip_n_frames)
    {
        n_skipped++;
        return;
    }
    else
        n_skipped = 0;

    if (pipe_client_bytes_in_pipe(ch) > 0)
    {
        n_skipped++;
        if (en_debug)
            fprintf(
                stderr,
                "WARNING, skipping frame on channel %d due to frame backup\n",
                ch);
        return;
    }
    if (!en_debug && !en_timing)
    {
        if (!pipe_server_get_num_clients(IMAGE_CH) &&
            !pipe_server_get_num_clients(DETECTION_CH))
            return;
    }

    if (meta.size_bytes > MAX_IMAGE_SIZE)
    {
        fprintf(stderr, "Model cannot process an image with %d bytes\n",
                meta.size_bytes);
        return;
    }

    int queue_ind = model_helper->camera_queue.insert_idx;

    TFLiteMessage *camera_message = &model_helper->camera_queue.queue[queue_ind];

    camera_message->metadata = meta;
    memcpy(camera_message->image_pixels, (uint8_t *)frame, meta.size_bytes);

    model_helper->camera_queue.insert_idx = ((queue_ind + 1) % QUEUE_SIZE);
    model_helper->cond_var.notify_all();

    // print timing if requested
    if (en_timing)
        model_helper->print_summary_stats();

    

    return;
}

static void set_delegate(DelegateOpt *opt)
{
    *opt = GPU; // default for MAI models
    if (!strcmp(delegate, "cpu"))
        *opt = XNNPACK;
    else if (!strcmp(delegate, "nnapi"))
        *opt = NNAPI;
}

static void initialize_model_settings(char *model, char *delegate, ModelName *model_name, ModelCategory *model_category, NormalizationType *norm_type)
{

    // set model type
    if (!strcmp(model, "/usr/bin/dnn/ssdlite_mobilenet_v2_coco.tflite"))
    {
        *model_name = MOBILE_NET;
        *model_category = OBJECT_DETECTION;
        // funky for mobilenet, doesn't like hard division
        *norm_type = PIXEL_MEAN;
    }
    else if (!strcmp(model, "/usr/bin/dnn/mobilenetv1_nnapi_quant.tflite"))
    {
        *model_name = MOBILE_NET;
        *model_category = OBJECT_DETECTION;
        // funky for mobilenet, doesn't like hard division
        *norm_type = PIXEL_MEAN;
    }
    else if (!strcmp(model, "/usr/bin/dnn/fastdepth_float16_quant.tflite"))
    {
        *model_name = FAST_DEPTH;
        *model_category = MONO_DEPTH;
        *norm_type = HARD_DIVISION;
    }
    else if (!strcmp(model,
                     "/usr/bin/dnn/"
                     "edgetpu_deeplab_321_os32_float16_quant.tflite"))
    {
        *model_name = DEEPLAB;
        *model_category = SEGMENTATION;
        *norm_type = NONE;
    }
    else if (!strcmp(model,
                     "/usr/bin/dnn/"
                     "lite-model_efficientnet_lite4_uint8_2.tflite"))
    {
        *model_name = EFFICIENT_NET;
        *model_category = CLASSIFICATION;
        *norm_type = PIXEL_MEAN;
    }
    else if (!strcmp(model,
                     "/usr/bin/dnn/mobilenetv1_nnapi_classifier.tflite"))
    {
        *model_name = MOBILE_NET;
        *model_category = CLASSIFICATION;
        *norm_type = PIXEL_MEAN;
    }
    else if (!strcmp(model,
                     "/usr/bin/dnn/"
                     "lite-model_movenet_singlepose_lightning_tflite_float16_"
                     "4.tflite"))
    {
        *model_name = POSENET;
        *model_category = POSE;
        *norm_type = NONE;
    }
    else if (!strcmp(model, "/usr/bin/dnn/yolov5_float16_quant.tflite"))
    {
        *model_name = YOLOV5;
        *model_category = OBJECT_DETECTION;
        *norm_type = HARD_DIVISION;
    }
    else if (!strcmp(model, "/usr/bin/dnn/yolov8n_float16.tflite"))
    {
        *model_name = YOLOV8;
        *model_category = OBJECT_DETECTION;
        *norm_type = HARD_DIVISION;
    }
    else if (!strcmp(model, "/usr/bin/dnn/yolov11n_float16.tflite"))
    {
        *model_name = YOLOV11;
        *model_category = OBJECT_DETECTION;
        *norm_type = HARD_DIVISION;
    }
    else if (!strcmp(model, "/usr/bin/dnn/gate_xyz.tflite"))
    {
        *model_name     = GATE_XYZ;
        *model_category = OBJECT_DETECTION;
        *norm_type      = NONE;
    }
    else if (!strcmp(model, "/usr/bin/dnn/gate_yaw.tflite"))
    {
        *model_name     = GATE_YAW;
        *model_category = OBJECT_DETECTION;
        *norm_type      = NONE;
    }
    else if (!strcmp(model, "/usr/bin/dnn/gate_bin.tflite"))
    {
        *model_name     = GATE_BIN;
        *model_category = OBJECT_DETECTION;
        *norm_type      = NONE;
    }
    else
    {
        fprintf(stderr,
                "WARNING: Unknown model type provided! Defaulting post-process "
                "to object detection.\n");
        *model_name = PLACEHOLDER;
        *model_category = OBJECT_DETECTION;
    }
}