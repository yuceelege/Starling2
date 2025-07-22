#ifndef GENERIC_CLASSIFICATION_H
#define GENERIC_CLASSIFICATION_H

#include "model_helper/model_helper.h"

class GenericClassificationModelHelper : public ModelHelper
{

public:
    GenericClassificationModelHelper(char *model_file, char *labels_file,
                                     DelegateOpt delegate_choice, bool _en_debug,
                                     bool _en_timing, NormalizationType _do_normalize, int tensor_offset);

    bool postprocess(cv::Mat &output_image, double last_inference_time, void *input_params) override;
    bool worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params) override;

private:
    std::vector<std::string> labels;
    size_t label_count;
    int tensor_offset;
};

#endif
