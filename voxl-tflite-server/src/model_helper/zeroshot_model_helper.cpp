// model_helper/zeroshot_model_helper.cpp
#include "model_helper/zeroshot_model_helper.h"

ZeroshotModelHelper::ZeroshotModelHelper(char *model_file,
                                       char *labels_file,
                                       DelegateOpt delegate_choice,
                                       bool _en_debug,
                                       bool _en_timing,
                                       NormalizationType _do_normalize)
  : ModelHelper(model_file, labels_file, delegate_choice, _en_debug, _en_timing, _do_normalize)
{
    control_buffer_filled = false;
    memset(control_input, 0, sizeof(control_input));
}

bool ZeroshotModelHelper::postprocess(cv::Mat &,
                                     double,
                                     void *)
{
    return true;
}

bool ZeroshotModelHelper::worker(cv::Mat &output_image,
                                 double last_inference_time,
                                 camera_image_metadata_t metadata,
                                 void *input_params)
{
    // Check if control data is available
    if (!control_buffer_filled) {
        if (en_debug) {
            fprintf(stderr, "Zeroshot: No control data available, skipping\n");
        }
        return true;
    }
    
    // Get output and create control message (like gate_xyz)
    const float *out = interpreter->typed_output_tensor<float>(0);
    ZeroshotMsg msg{
        out[0],  // vx
        out[1],  // vy
        out[2],  // vz
        out[3],  // yaw
        static_cast<uint64_t>(metadata.timestamp_ns)
    };
    
    if (en_debug) {
        fprintf(stdout,"Zeroshot → vx=%.6f vy=%.6f vz=%.6f yaw=%.6f ts=%llu\n",
                msg.vx, msg.vy, msg.vz, msg.yaw, (unsigned long long)msg.timestamp_ns);
    }
    
    pipe_server_write(DETECTION_CH, &msg, sizeof(msg));
    return true;
}

cv::Mat ZeroshotModelHelper::combine_rgb_and_controls(const cv::Mat &rgb_image, const float control_buffer[5][4])
{
    // Convert RGB image to float32 and normalize to [0,1]
    cv::Mat rgb_float;
    rgb_image.convertTo(rgb_float, CV_32F, 1.0/255.0);
    
    // Reshape RGB to 1D vector: (height * width * channels)
    int rgb_size = rgb_float.rows * rgb_float.cols * rgb_float.channels();
    cv::Mat rgb_1d = rgb_float.reshape(1, rgb_size);
    
    // Create control buffer as 1D vector: (5 * 4 = 20 elements)
    cv::Mat control_1d(20, 1, CV_32F);
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 4; j++) {
            control_1d.at<float>(i * 4 + j, 0) = control_buffer[i][j];
        }
    }
    
    // Concatenate RGB and control vectors
    cv::Mat combined;
    cv::vconcat(rgb_1d, control_1d, combined);
    
    if (en_debug) {
        fprintf(stderr, "Combined input size: %d (RGB: %d + Controls: 20)\n", 
                combined.rows, rgb_size);
    }
    
    return combined;
}

bool ZeroshotModelHelper::run_inference(cv::Mat &preprocessed_image, double *last_inference_time)
{
    // Check if control data is available
    if (!control_buffer_filled) {
        if (en_debug) {
            fprintf(stderr, "Zeroshot: No control data available for inference\n");
        }
        return false;
    }
    
    // Combine RGB image with control buffer
    cv::Mat combined_input = combine_rgb_and_controls(preprocessed_image, control_input);
    
    // Get input tensor
    TfLiteTensor *input_tensor = interpreter->input_tensor(0);
    if (!input_tensor) {
        fprintf(stderr, "Failed to get input tensor\n");
        return false;
    }
    
    // Check if input size matches
    int expected_size = input_tensor->bytes / sizeof(float);
    int actual_size = combined_input.rows;
    
    if (expected_size != actual_size) {
        fprintf(stderr, "Input size mismatch: expected %d, got %d\n", expected_size, actual_size);
        return false;
    }
    
    // Copy combined input to tensor
    float *input_data = interpreter->typed_input_tensor<float>(0);
    memcpy(input_data, combined_input.data, combined_input.rows * sizeof(float));
    
    // Run inference
    TfLiteStatus status = interpreter->Invoke();
    if (status != kTfLiteOk) {
        fprintf(stderr, "Inference failed with status %d\n", status);
        return false;
    }
    
    *last_inference_time = rc_nanos_monotonic_time();
    return true;
}

void ZeroshotModelHelper::update_control_buffer(const float control_buffer[5][4], bool filled)
{
    memcpy(control_input, control_buffer, sizeof(control_input));
    control_buffer_filled = filled;
} 