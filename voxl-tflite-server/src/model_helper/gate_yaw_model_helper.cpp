// model_helper/gate_yaw_model_helper.cpp
#include "model_helper/gate_yaw_model_helper.h"

struct GateYawMsg {
    float yaw;
    uint64_t timestamp_ns;
};

GateYawModelHelper::GateYawModelHelper(char *model_file,
                                       char *labels_file,
                                       DelegateOpt delegate_choice,
                                       bool _en_debug,
                                       bool _en_timing,
                                       NormalizationType _do_normalize)
  : ModelHelper(model_file, labels_file, delegate_choice, _en_debug, _en_timing, _do_normalize)
{}

bool GateYawModelHelper::postprocess(cv::Mat &,
                                     double,
                                     void *)
{
    return true;
}

bool GateYawModelHelper::worker(cv::Mat &output_image,
                                double last_inference_time,
                                camera_image_metadata_t metadata,
                                void * /*input_params*/)
{
    const float *out = interpreter->typed_output_tensor<float>(0);
    GateYawMsg msg{
        out[0],
        static_cast<uint64_t>(metadata.timestamp_ns)
    };
    pipe_server_write(DETECTION_CH, &msg, sizeof(msg));
    return true;
}
