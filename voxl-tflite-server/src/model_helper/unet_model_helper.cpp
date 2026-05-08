#include "model_helper/unet_model_helper.h"
#include "tensor_data.h"
#include "image_utils.h"
#include <sys/stat.h>
#include <unistd.h>

void log_memory_usage(const char* location) {
    FILE* status = fopen("/proc/self/status", "r");
    if (status) {
        char line[256];
        while (fgets(line, sizeof(line), status)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                fprintf(stderr, "MEMORY [%s]: %s", location, line);
                break;
            }
        }
        fclose(status);
    }
}

UnetModelHelper::UnetModelHelper(char *model_file, char *labels_file,
                                 DelegateOpt delegate_choice, bool _en_debug,
                                 bool _en_timing, NormalizationType _do_normalize)
    : ModelHelper(model_file, labels_file, delegate_choice, _en_debug, _en_timing, _do_normalize),
      logging_enabled(false), frame_counter(0)
{
    // No dimension override - let the model define its own dimensions
    // No labels needed for UNet - just threshold-based masking
    
    // Initialize logging
    //initialize_logging();
}

void UnetModelHelper::initialize_logging()
{
    log_dir = "/projectlog";
    
    // Create log directory if it doesn't exist
    struct stat st = {0};
    if (stat(log_dir.c_str(), &st) == -1) {
        if (mkdir(log_dir.c_str(), 0755) == 0) {
            fprintf(stderr, "DEBUG: Created logging directory: %s\n", log_dir.c_str());
        } else {
            fprintf(stderr, "DEBUG: Failed to create logging directory: %s\n", log_dir.c_str());
            logging_enabled = false;
            return;
        }
    }
    
    fprintf(stderr, "DEBUG: UNet logging initialized - directory: %s\n", log_dir.c_str());
}

std::string UnetModelHelper::get_timestamp_string()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    ss << "_frame" << std::setfill('0') << std::setw(6) << frame_counter;
    
    return ss.str();
}

void UnetModelHelper::log_synchronized_rgb(const cv::Mat& rgb_image, const std::string& timestamp)
{
    if (!logging_enabled) return;
    
    // Create timestamp subfolder
    std::string timestamp_dir = log_dir + "/" + timestamp;
    struct stat st = {0};
    if (stat(timestamp_dir.c_str(), &st) == -1) {
        mkdir(timestamp_dir.c_str(), 0755);
    }
    
    std::string filename = timestamp_dir + "/synchronized_rgb.png";
    cv::imwrite(filename, rgb_image);
    fprintf(stderr, "DEBUG: Logged synchronized RGB: %s\n", filename.c_str());
}

void UnetModelHelper::log_resized_image(const cv::Mat& resized_image, const std::string& timestamp)
{
    if (!logging_enabled) return;
    
    // Create timestamp subfolder
    std::string timestamp_dir = log_dir + "/" + timestamp;
    struct stat st = {0};
    if (stat(timestamp_dir.c_str(), &st) == -1) {
        mkdir(timestamp_dir.c_str(), 0755);
    }
    
    std::string filename = timestamp_dir + "/resized_image.png";
    cv::imwrite(filename, resized_image);
    fprintf(stderr, "DEBUG: Logged resized image: %s\n", filename.c_str());
}

void UnetModelHelper::log_inference_output(float* output_data, int batch, int height, int width, int channels, const std::string& timestamp)
{
    if (!logging_enabled || !output_data) return;
    
    // Create timestamp subfolder
    std::string timestamp_dir = log_dir + "/" + timestamp;
    struct stat st = {0};
    if (stat(timestamp_dir.c_str(), &st) == -1) {
        mkdir(timestamp_dir.c_str(), 0755);
    }
    
    // Log both channels as separate images
    for (int ch = 0; ch < channels; ch++) {
        cv::Mat channel_image(height, width, CV_32FC1);
        
        for (int h = 0; h < height; h++) {
            for (int w = 0; w < width; w++) {
                int index = (h * width * channels) + (w * channels) + ch;
                channel_image.at<float>(h, w) = output_data[index];
            }
        }
        
        // Normalize to 0-255 for visualization
        cv::Mat normalized;
        cv::normalize(channel_image, normalized, 0, 255, cv::NORM_MINMAX, CV_8UC1);
        
        std::string filename = timestamp_dir + "/inference_ch" + std::to_string(ch) + ".png";
        cv::imwrite(filename, normalized);
        fprintf(stderr, "DEBUG: Logged inference channel %d: %s\n", ch, filename.c_str());
    }
    
    // Also log raw data as binary file
    std::string raw_filename = timestamp_dir + "/inference_raw.bin";
    std::ofstream raw_file(raw_filename, std::ios::binary);
    if (raw_file.is_open()) {
        raw_file.write(reinterpret_cast<const char*>(output_data), 
                      batch * height * width * channels * sizeof(float));
        raw_file.close();
        fprintf(stderr, "DEBUG: Logged raw inference data: %s\n", raw_filename.c_str());
    }
}

void UnetModelHelper::log_postprocess_output(const cv::Mat& output_image, const std::string& timestamp)
{
    if (!logging_enabled) return;
    
    // Create timestamp subfolder
    std::string timestamp_dir = log_dir + "/" + timestamp;
    struct stat st = {0};
    if (stat(timestamp_dir.c_str(), &st) == -1) {
        mkdir(timestamp_dir.c_str(), 0755);
    }
    
    std::string filename = timestamp_dir + "/postprocess_output.png";
    cv::imwrite(filename, output_image);
    fprintf(stderr, "DEBUG: Logged postprocess output: %s\n", filename.c_str());
}

bool UnetModelHelper::preprocess(camera_image_metadata_t &meta,
                                 char *frame, std::shared_ptr<cv::Mat> preprocessed_image,
                                 std::shared_ptr<cv::Mat> output_image)
{
    log_memory_usage("UNET_PREPROCESS_START");
    
    // Call parent preprocess first to do all the actual preprocessing work
    bool success = ModelHelper::preprocess(meta, frame, preprocessed_image, output_image);
    if (!success) {
        return false;
    }
    
    // Store the original RGB image from output_image (this is the full-size RGB before resizing)
    if (output_image) {
        original_rgb_image = output_image->clone();
    }
    
    log_memory_usage("UNET_PREPROCESS_END");
    return true;
}

bool UnetModelHelper::worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params)
{
    log_memory_usage("UNET_WORKER_START");
    
    // Generate timestamp for this frame
    current_timestamp = get_timestamp_string();

    // Log synchronized RGB image (original camera image before resizing)
    //log_synchronized_rgb(original_rgb_image, current_timestamp);
    
    // Log resized image (224x224 input to model)
    //log_resized_image(*preprocessed_image, current_timestamp);
    
    // Create DeepLabModelParams for metadata
    DeepLabModelParams params(metadata);
    
    // Call postprocess to generate the output
    bool success = postprocess(output_image, last_inference_time, &params);
    if (!success) {
        return false;
    }
    
    // Write the binary mask to the UNet pipe
    pipe_server_write_camera_frame(IMAGE_CH, params.meta, (char *)output_image.data);
    
    // Increment frame counter
    frame_counter++;
    
    log_memory_usage("UNET_WORKER_END");
    return true;
}

bool UnetModelHelper::postprocess(cv::Mat &output_image, double last_inference_time, void *input_params)
{
    log_memory_usage("UNET_POSTPROCESS_START");
    
    // Cast input params
    DeepLabModelParams *params = static_cast<DeepLabModelParams*>(input_params);
    if (!params) {
        return false;
    }
    
    // Get output tensor
    TfLiteTensor *output_tensor = interpreter->output_tensor(0);
    if (!output_tensor) {
        return false;
    }
    
    // Parse dimensions as NHWC: [batch, height, width, channels]
    int height = output_tensor->dims->data[1];
    int width = output_tensor->dims->data[2];
    int num_channels = output_tensor->dims->data[3];
    
    // Get tensor data
    float *output_data = TensorData<float>(output_tensor, 0);
    if (!output_data) {
        return false;
    }
    
    // Create binary mask from second channel (index 1) with softmax
    cv::Mat temp(height, width, CV_8UC1);
    
    // Compare both channels and choose the largest one for each pixel (argmax)
    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            // Get logits for both channels at this pixel
            int index_ch0 = (h * width * num_channels) + (w * num_channels) + 0;
            int index_ch1 = (h * width * num_channels) + (w * num_channels) + 1;
            float logit_ch0 = output_data[index_ch0];
            float logit_ch1 = output_data[index_ch1];
            
            // Choose the channel with higher logit value (argmax)
            uchar mask_value = (logit_ch1 > logit_ch0) ? 255 : 0;
            
            // Set single channel value (grayscale)
            temp.at<uchar>(h, w) = mask_value;
        }
    }
    
    // Convert grayscale to RGB
    cv::Mat temp_rgb;
    cv::cvtColor(temp, temp_rgb, cv::COLOR_GRAY2BGR);
    
    // Set up output metadata
    params->meta.width = temp_rgb.cols;
    params->meta.height = temp_rgb.rows;
    params->meta.format = IMAGE_FORMAT_RGB;
    params->meta.size_bytes = temp_rgb.total() * temp_rgb.elemSize();
    params->meta.stride = temp_rgb.cols * temp_rgb.elemSize();
    
    // Copy the final image to output_image
    output_image = temp_rgb;
    
    log_memory_usage("UNET_POSTPROCESS_END");
    return true;
} 