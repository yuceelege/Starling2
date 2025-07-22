// model_helper/gate_bin_model_helper.h
#ifndef GATE_BIN_MODEL_HELPER_H
#define GATE_BIN_MODEL_HELPER_H

#include "model_helper/model_helper.h"

class GateBinModelHelper : public ModelHelper {
public:
    GateBinModelHelper(char *model_file,
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
};

#endif
