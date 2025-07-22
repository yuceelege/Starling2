#ifndef YOLOV5_H
#define YOLOV5_H

#include "model_helper/model_helper.h"

class YoloV5ModelHelper : public ModelHelper
{
public:
    YoloV5ModelHelper(char *model_file, char *labels_file,
                      DelegateOpt delegate_choice, bool _en_debug,
                      bool _en_timing, NormalizationType _do_normalize);
    bool postprocess(cv::Mat &output_image, double last_inference_time, void *input_params) override;
    bool worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params) override;

private:
    std::vector<std::string> labels;
    size_t label_count;
    // straight up stolen from
    // https://github.com/iwatake2222/play_with_tflite/blob/master/pj_tflite_det_yolov5/image_processor/detection_engine.cpp
    static constexpr int32_t kGridScaleList[3] = {8, 16, 32};
    static constexpr int32_t kGridChannel = 3;
    static constexpr float threshold_box_confidence_ =
        0.40; // not sure if this is too low or high yet
    static constexpr float threshold_class_confidence_ =
        0.20; // not sure if this is too low or high yet
    static constexpr float threshold_nms_iou_ =
        0.50; // not sure if this is too low or high yet

    int32_t kElementNumOfAnchor;
    int32_t kNumberOfClass;

    std::vector<ai_detection_t> detections_vector;

    struct b_box
    {
        int32_t class_id;
        std::string label;
        float class_conf;
        float detection_conf;
        float score;
        int32_t x;
        int32_t y;
        int32_t w;
        int32_t h;
    };

    void get_bbox(const float *data, float scale_x, float scale_y,
                  int32_t grid_w, int32_t grid_h, int number_of_classes,
                  std::vector<b_box> &bbox_list);
    void nms(std::vector<b_box> &bbox_list,
             std::vector<b_box> &bbox_nms_list, float threshold_nms_iou,
             bool check_class_id);
    float calc_iou(const b_box &obj0, const b_box &obj1);
};

#endif
