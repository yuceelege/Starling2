#ifndef MODEL_INFO_H
#define MODEL_INFO_H

// #include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <modal_pipe.h>
// #include <stdint.h>
#include <memory>
// #include <opencv2/imgproc/types_c.h>
#include "ai_detection.h"

enum ModelName
{
    MOBILE_NET,
    FAST_DEPTH,
    DEEPLAB,
    EFFICIENT_NET,
    POSENET,
    YOLOV5,
    YOLOV8,
    YOLOV11,
    GATE_XYZ,
    GATE_YAW,
    GATE_BIN,
    PLACEHOLDER
};

// The category enum is a kinda redundant, might get rid
// of it all together
enum ModelCategory
{
    OBJECT_DETECTION,
    CLASSIFICATION,
    SEGMENTATION,
    MONO_DEPTH,
    POSE
};

class GenericObjectDetectionModelParams
{
public:
    std::vector<ai_detection_t> detections_vector;

    // Constructor to initialize detections_vector
    GenericObjectDetectionModelParams(const std::vector<ai_detection_t> &detections)
        : detections_vector(detections) {}
};

class ClassificationModelParams
{
public:
    int tensor_offset;

    ClassificationModelParams(int tensor_offset)
        : tensor_offset(tensor_offset) {}
};

class FastDepthModelParams
{
public:
    camera_image_metadata_t &meta;

    FastDepthModelParams(camera_image_metadata_t &meta_)
        : meta(meta_) {} 
};

class DeepLabModelParams
{
public:
    camera_image_metadata_t &meta;

    DeepLabModelParams(camera_image_metadata_t &meta_)
        : meta(meta_) {}
};

#endif