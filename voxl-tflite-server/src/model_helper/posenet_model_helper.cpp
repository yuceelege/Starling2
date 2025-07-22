#include "model_helper/posenet_model_helper.h"
#include "tensor_data.h"
#include "image_utils.h"

static const std::vector<std::pair<int32_t, int32_t>> kJointLineList{
    /* face */
    {0, 2},
    {2, 4},
    {0, 1},
    {1, 3},
    /* body */
    {6, 5},
    {5, 11},
    {11, 12},
    {12, 6},
    /* arm */
    {6, 8},
    {8, 10},
    {5, 7},
    {7, 9},
    /* leg */
    {12, 14},
    {14, 16},
    {11, 13},
    {13, 15},
};

PoseNetModelHelper::PoseNetModelHelper(char *model_file, char *labels_file,
                                       DelegateOpt delegate_choice, bool _en_debug,
                                       bool _en_timing, NormalizationType _do_normalize)
    : ModelHelper(model_file, labels_file, delegate_choice, _en_debug, _en_timing, _do_normalize) {}

bool PoseNetModelHelper::postprocess(cv::Mat &output_image, double last_inference_time, void *input_params)
{
    start_time = rc_nanos_monotonic_time();

    float confidence_threshold = 0.2;

    TfLiteTensor *output_locations =
        interpreter->tensor(interpreter->outputs()[0]);
    float *pose_tensor = TensorData<float>(output_locations, 0);

    std::vector<int32_t> x_coords;
    std::vector<int32_t> y_coords;
    std::vector<float> confidences;

    for (int i = 0; i < 17; i++)
    {
        x_coords.push_back(
            static_cast<int32_t>(pose_tensor[i * 3 + 1] * input_width));
        y_coords.push_back(
            static_cast<int32_t>(pose_tensor[i * 3] * input_height));
        confidences.push_back(pose_tensor[i * 3 + 2]);
    }

    for (const auto &jointLine : kJointLineList)
    {
        if (confidences[jointLine.first] >= confidence_threshold &&
            confidences[jointLine.second] >= confidence_threshold)
        {
            int32_t x0 = x_coords[jointLine.first];
            int32_t y0 = y_coords[jointLine.first];
            int32_t x1 = x_coords[jointLine.second];
            int32_t y1 = y_coords[jointLine.second];
            cv::line(output_image, cv::Point(x0, y0), cv::Point(x1, y1),
                     cv::Scalar(200, 200, 200), 2);
        }
    }

    for (unsigned int i = 0; i < x_coords.size(); i++)
    {
        if (confidences[i] > confidence_threshold)
        {
            cv::circle(output_image, cv::Point(x_coords[i], y_coords[i]), 4,
                       cv::Scalar(255, 255, 0), cv::FILLED);
        }
    }

    draw_fps(output_image, last_inference_time, cv::Point(0, 0), 0.5, 2,
             cv::Scalar(0, 0, 0), cv::Scalar(180, 180, 180), true);

    return true;
}

bool PoseNetModelHelper::worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params)
{
    if (!postprocess(output_image, last_inference_time, input_params))
        return false;
    metadata.timestamp_ns = rc_nanos_monotonic_time();
    pipe_server_write_camera_frame(IMAGE_CH, metadata,
                                   (char *)output_image.data);
    return true;
}