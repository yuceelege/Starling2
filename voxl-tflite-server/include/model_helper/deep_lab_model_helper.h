#ifndef DEEP_LAB_H
#define DEEP_LAB_H

#include "model_helper/model_helper.h"

class DeepLabModelHelper : public ModelHelper
{
public:
    DeepLabModelHelper(char *model_file, char *labels_file,
                       DelegateOpt delegate_choice, bool _en_debug,
                       bool _en_timing, NormalizationType _do_normalize);
    bool postprocess(cv::Mat &output_image, double last_inference_time, void *input_params) override;
    bool worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params) override;

private:
    static constexpr int right_pixel_border = 110;
    std::vector<std::string> labels;
    size_t label_count;
    camera_image_metadata_t new_frame_metadata;
};

#endif
