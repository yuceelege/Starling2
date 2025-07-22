#include "model_helper/fast_depth_model_helper.h"
#include "tensor_data.h"
#include "image_utils.h"

FastDepthModelHelper::FastDepthModelHelper(char *model_file, char *labels_file,
                                           DelegateOpt delegate_choice, bool _en_debug,
                                           bool _en_timing, NormalizationType _do_normalize)
    : ModelHelper(model_file, labels_file, delegate_choice, _en_debug, _en_timing, _do_normalize) {}

bool FastDepthModelHelper::postprocess(cv::Mat &output_image, double last_inference_time, void *input_params)
{

    FastDepthModelParams *params = static_cast<FastDepthModelParams*>(input_params);

    start_time = rc_nanos_monotonic_time();

    TfLiteTensor *output_locations = interpreter->tensor(interpreter->outputs()[0]);
    float *depth = TensorData<float>(output_locations, 0);

    // actual depth image if desired
    cv::Mat depthImage(model_height, model_width, CV_32FC1, depth);

    // setup output metadata
    params->meta.height = model_height;
    params->meta.width = model_width;
    params->meta.size_bytes = params->meta.width * params->meta.height * 3;
    params->meta.stride = params->meta.width * 3;
    params->meta.format = IMAGE_FORMAT_RGB;

    // create a pretty colored depth image from the data
    double min_val, max_val;
    cv::Mat depthmap_visual;
    cv::minMaxLoc(depthImage, &min_val, &max_val);
    depthmap_visual = 255 * (depthImage - min_val) /
                      (max_val - min_val); // * 255 for "scaled" disparity
    depthmap_visual.convertTo(depthmap_visual, CV_8U);
    cv::applyColorMap(depthmap_visual, output_image, 4); // opencv COLORMAP_JET

    if (en_timing)
        total_postprocess_time +=
            ((rc_nanos_monotonic_time() - start_time) / 1000000.);

    draw_fps(output_image, last_inference_time, cv::Point(0, 0), 0.5, 2,
             cv::Scalar(0, 0, 0), cv::Scalar(180, 180, 180), true);

    return true;
}

bool FastDepthModelHelper::worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params)
{
    FastDepthModelParams* params = new FastDepthModelParams(metadata);

    if (!postprocess(output_image, last_inference_time, params)) {
        return false;
    }
    params->meta.timestamp_ns = rc_nanos_monotonic_time();


    pipe_server_write_camera_frame(IMAGE_CH, params->meta,
                                   (char *)output_image.data);

    delete params;
    return true;
}