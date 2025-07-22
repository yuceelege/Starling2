#include "model_helper/deep_lab_model_helper.h"
#include "tensor_data.h"
#include "image_utils.h"

// pre-defined color map for each class, corresponds to cityscapes_labels.txt
static const uint8_t color_map[57] = {
    139, 0, 0, 255, 0, 0, 255, 99, 71, 250, 128, 114, 255, 140, 0,
    255, 255, 0, 189, 183, 107, 154, 205, 50, 0, 255, 0, 0, 100, 0,
    0, 250, 154, 0, 128, 128, 30, 144, 255, 25, 25, 112, 138, 43, 226,
    75, 0, 130, 139, 0, 139, 238, 130, 238, 255, 20, 147};

DeepLabModelHelper::DeepLabModelHelper(char *model_file, char *labels_file,
                                       DelegateOpt delegate_choice, bool _en_debug,
                                       bool _en_timing, NormalizationType _do_normalize)
    : ModelHelper(model_file, labels_file, delegate_choice, _en_debug, _en_timing, _do_normalize)
{
    if (labels.empty())
    {
        if (ReadLabelsFile(labels_location, &labels, &label_count) !=
            kTfLiteOk)
        {
            fprintf(stderr, "ERROR: Unable to read labels file\n");
            exit(-1);
        }
    }
}

bool DeepLabModelHelper::worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params)
{
    // Segmentation is a special case here
    // instead of passing the full dimension "output_image", we pass the
    // preprocessed_image back then, the model output and overlay image
    // are the same dims so we can easily blend the two

    // passing output_image but it will not be used since preprocessed_image is already
    // a class member
    DeepLabModelParams* params = new DeepLabModelParams(metadata);

    if (!postprocess(output_image, last_inference_time, params)) {
        return false;
    }

    params->meta.timestamp_ns = rc_nanos_monotonic_time();
    pipe_server_write_camera_frame(IMAGE_CH, params->meta,
                                   (char *)preprocessed_image->data);
    return true;
}

bool DeepLabModelHelper::postprocess(cv::Mat& output_image, double last_inference_time, void *input_params)
{
    DeepLabModelParams* params = static_cast<DeepLabModelParams*>(input_params);

    start_time = rc_nanos_monotonic_time();

    TfLiteTensor *output_locations =
        interpreter->tensor(interpreter->outputs()[0]);

    int64_t *classes = TensorData<int64_t>(output_locations, 0);

    cv::Mat temp(model_height, model_width, CV_8UC3, cv::Scalar(0, 0, 0));

    for (int i = 0; i < model_width; i++)
    {
        for (int j = 0; j < model_height; j++)
        {
            cv::Vec3b color = temp.at<cv::Vec3b>(cv::Point(j, i));

            color[0] = color_map[classes[(i * model_width) + j] * 3];
            color[1] = color_map[classes[(i * model_width) + j] * 3 + 1];
            color[2] = color_map[classes[(i * model_width) + j] * 3 + 2];
            temp.at<cv::Vec3b>(cv::Point(j, i)) = color;
        }
    }

    // now blend the model input and output
    cv::addWeighted(*preprocessed_image, 0.75, temp, 0.25, 0, *preprocessed_image);
    // add key overlay
    cv::copyMakeBorder(*preprocessed_image, *preprocessed_image, 0, 0, 0, right_pixel_border,
                       cv::BORDER_CONSTANT);

    for (unsigned int i = 0; i < labels.size(); i++)
    {
        cv::putText(*preprocessed_image, labels[i], cv::Point(325, 16 * (i + 1)),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4,
                    cv::Scalar(color_map[(i * 3)], color_map[(i * 3) + 1],
                               color_map[(i * 3) + 2]),
                    1);
    }

    // now, setup metadata since we modified the output image
    params->meta.format = IMAGE_FORMAT_RGB;
    params->meta.width = model_width + right_pixel_border;
    params->meta.height = model_height;
    params->meta.stride = params->meta.width * 3;
    params->meta.size_bytes = params->meta.height * params->meta.width * 3;

    draw_fps(*preprocessed_image, last_inference_time, cv::Point(0, 0), 0.25, 0.4,
             cv::Scalar(0, 0, 0), cv::Scalar(180, 180, 180), true);

    if (en_timing)
        total_postprocess_time +=
            ((rc_nanos_monotonic_time() - start_time) / 1000000.);

    return true;

}