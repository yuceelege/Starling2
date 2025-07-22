#ifndef YOLOV8_H
#define YOLOV8_H

#include "model_helper/model_helper.h"

class YoloV8ModelHelper : public ModelHelper
{
public:
    YoloV8ModelHelper(char *model_file, char *labels_file,
                      DelegateOpt delegate_choice, bool _en_debug,
                      bool _en_timing, NormalizationType _do_normalize);
    bool postprocess(cv::Mat &output_image, double last_inference_time, void *input_params) override;
    bool preprocess(camera_image_metadata_t &meta,
                    char *frame, std::shared_ptr<cv::Mat> preprocessed_image,
                    std::shared_ptr<cv::Mat> output_image);
    bool run_inference(cv::Mat &preprocessed_image,
                       double *last_inference_time) override;
    bool worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params) override;

private:
    std::vector<std::string> labels;
    size_t label_count;
    std::vector<ai_detection_t> detections_vector;

    const float model_score_threshold = 0.45;
    const float model_confidence_threshold = 0.25;
    const float model_nms_threshold = 0.5;
};

#endif