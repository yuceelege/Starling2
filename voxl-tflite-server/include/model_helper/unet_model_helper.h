#ifndef UNET_MODEL_HELPER_H
#define UNET_MODEL_HELPER_H

#include "model_helper/model_helper.h"
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

class UnetModelHelper : public ModelHelper
{
public:
    UnetModelHelper(char *model_file, char *labels_file,
                    DelegateOpt delegate_choice, bool _en_debug,
                    bool _en_timing, NormalizationType _do_normalize);
    bool preprocess(camera_image_metadata_t &meta,
                    char *frame, std::shared_ptr<cv::Mat> preprocessed_image,
                    std::shared_ptr<cv::Mat> output_image) override;
    bool postprocess(cv::Mat &output_image, double last_inference_time, void *input_params) override;
    bool worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params) override;

private:
    static constexpr int right_pixel_border = 110;
    camera_image_metadata_t new_frame_metadata;
    
    // Logging functionality
    bool logging_enabled;
    std::string log_dir;
    int frame_counter;
    cv::Mat original_rgb_image;  // Store original RGB image for logging
    std::string current_timestamp;  // Store current frame timestamp
    
    // Logging methods
    void initialize_logging();
    std::string get_timestamp_string();
    void log_synchronized_rgb(const cv::Mat& rgb_image, const std::string& timestamp);
    void log_resized_image(const cv::Mat& resized_image, const std::string& timestamp);
    void log_inference_output(float* output_data, int batch, int height, int width, int channels, const std::string& timestamp);
    void log_postprocess_output(const cv::Mat& output_image, const std::string& timestamp);
};

#endif