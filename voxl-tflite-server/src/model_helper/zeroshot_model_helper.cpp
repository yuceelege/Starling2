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

bool ZeroshotModelHelper::preprocess(camera_image_metadata_t &meta,
                                    char *frame, 
                                    std::shared_ptr<cv::Mat> preprocessed_image,
                                    std::shared_ptr<cv::Mat> output_image)
{
    // Let the base class handle image preprocessing and resizing
    // This will automatically resize the image to model_width x model_height x 3
    return ModelHelper::preprocess(meta, frame, preprocessed_image, output_image);
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



bool ZeroshotModelHelper::run_inference(cv::Mat &preprocessed_image, double *last_inference_time)
{
    // Check if control data is available
    if (!control_buffer_filled) {
        if (en_debug) {
            fprintf(stderr, "Zeroshot: No control data available for inference\n");
        }
        return false;
    }
    
    // Get input tensors
    TfLiteTensor *image_tensor = interpreter->input_tensor(0);  // RGB image input
    TfLiteTensor *control_tensor = interpreter->input_tensor(1); // Control input
    
    if (!image_tensor || !control_tensor) {
        fprintf(stderr, "Failed to get input tensors\n");
        return false;
    }
    
    if (en_debug) {
        fprintf(stderr, "Image tensor: %dx%dx%d, Control tensor: %d elements\n",
                image_tensor->dims->data[1], image_tensor->dims->data[2], image_tensor->dims->data[3],
                control_tensor->dims->data[1]);
    }
    
    // Copy resized image to first input tensor (RGB)
    float *image_data = interpreter->typed_input_tensor<float>(0);
    cv::Mat image_float;
    preprocessed_image.convertTo(image_float, CV_32F, 1.0/255.0); // Normalize to [0,1]
    
    // Verify image size matches expected tensor size
    int expected_image_size = image_tensor->dims->data[1] * image_tensor->dims->data[2] * image_tensor->dims->data[3];
    int actual_image_size = image_float.rows * image_float.cols * image_float.channels();
    
    if (expected_image_size != actual_image_size) {
        fprintf(stderr, "Image size mismatch: expected %d, got %d\n", expected_image_size, actual_image_size);
        return false;
    }
    
    // Safer copy - ensure continuous memory layout
    if (image_float.isContinuous()) {
        memcpy(image_data, image_float.data, actual_image_size * sizeof(float));
    } else {
        // If not continuous, copy row by row
        for (int i = 0; i < image_float.rows; i++) {
            memcpy(image_data + i * image_float.cols * image_float.channels(),
                   image_float.ptr<float>(i),
                   image_float.cols * image_float.channels() * sizeof(float));
        }
    }
    
    // Copy control data to second input tensor (5x4 controls)
    float *control_data = interpreter->typed_input_tensor<float>(1);
    int expected_control_size = control_tensor->dims->data[1];
    
    if (expected_control_size != 20) { // 5*4 = 20
        fprintf(stderr, "Control tensor size mismatch: expected %d, got 20\n", expected_control_size);
        return false;
    }
    
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 4; j++) {
            control_data[i * 4 + j] = control_input[i][j];
        }
    }
    
    if (en_debug) {
        fprintf(stderr, "Copied %d image elements and %d control elements to tensors\n", 
                actual_image_size, expected_control_size);
    }
    
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