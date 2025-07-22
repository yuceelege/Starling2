#ifndef GENERIC_OBJECT_DETECTION_H
#define GENERIC_OBJECT_DETECTION_H

#include "model_helper/model_helper.h"

class GenericObjectDetectionModelHelper : public ModelHelper
{
private:
    std::vector<std::string> labels;
    size_t label_count;
    std::vector<ai_detection_t> detections_vector;

public:
    GenericObjectDetectionModelHelper(char *model_file, char *labels_file,
                                      DelegateOpt delegate_choice, bool _en_debug,
                                      bool _en_timing, NormalizationType _do_normalize);

    bool postprocess(cv::Mat &output_image, double last_inference_time, void *input_params) override;
    bool worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params) override;
};

#endif