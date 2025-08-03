// model_helper/zeroshot_model_helper.h
#ifndef ZEROSHOT_MODEL_HELPER_H
#define ZEROSHOT_MODEL_HELPER_H

#include "model_helper/model_helper.h"

struct ZeroshotMsg {
    float vx, vy, vz, yaw;
    uint64_t timestamp_ns;
};

class ZeroshotModelHelper : public ModelHelper {
public:
    ZeroshotModelHelper(char *model_file,
                       char *labels_file,
                       DelegateOpt delegate_choice,
                       bool _en_debug,
                       bool _en_timing,
                       NormalizationType _do_normalize);
    bool worker(cv::Mat &output_image,
                double last_inference_time,
                camera_image_metadata_t metadata,
                void *input_params) override;
    bool postprocess(cv::Mat &output_image,
                     double last_inference_time,
                     void *input_params) override;
    bool run_inference(cv::Mat &preprocessed_image, double *last_inference_time) override;
private:
    float control_input[5][4];  // 5 past controls, each with vx,vy,vz,yaw
    bool control_buffer_filled;
    cv::Mat combine_rgb_and_controls(const cv::Mat &rgb_image, const float control_buffer[5][4]);
public:
    void update_control_buffer(const float control_buffer[5][4], bool filled);
};

#endif 