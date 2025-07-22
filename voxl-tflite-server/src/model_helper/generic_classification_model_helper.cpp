#include "model_helper/generic_classification_model_helper.h"
#include "tensor_data.h"
#include "image_utils.h"

GenericClassificationModelHelper::GenericClassificationModelHelper(char *model_file, char *labels_file,
                                                                   DelegateOpt delegate_choice, bool _en_debug,
                                                                   bool _en_timing, NormalizationType _do_normalize, int tensor_offset)
    : ModelHelper(model_file, labels_file, delegate_choice, _en_debug, _en_timing, _do_normalize)
{
    this->tensor_offset = tensor_offset;
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

bool GenericClassificationModelHelper::worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params)
{
    if (!postprocess(output_image, last_inference_time, input_params))
        return false;

    metadata.timestamp_ns = rc_nanos_monotonic_time();
    pipe_server_write_camera_frame(IMAGE_CH, metadata,
                                   (char *)output_image.data);
    return true;
}

bool GenericClassificationModelHelper::postprocess(cv::Mat &output_image, double last_inference_time, void *input_params)
{
    int num_of_classes = 1000;

    start_time = rc_nanos_monotonic_time();

    TfLiteTensor *output_locations =
        interpreter->tensor(interpreter->outputs()[0]);
    uint8_t *confidence_tensor = TensorData<uint8_t>(output_locations, 0);

    std::vector<uint8_t> confidences;
    confidences.assign(
        confidence_tensor + tensor_offset,
        confidence_tensor + num_of_classes + tensor_offset);

    uint8_t best_prob =
        *std::max_element(confidences.begin(), confidences.end());
    int best_class = std::max_element(confidences.begin(), confidences.end()) -
                     confidences.begin();

    fprintf(stderr, "class: %s, prob: %d\n", labels[best_class].c_str(),
            best_prob);
    cv::putText(output_image, labels[best_class],
                cv::Point(input_width / 3, 25), cv::FONT_HERSHEY_SIMPLEX, 0.8,
                cv::Scalar(0, 255, 0), 1);

    draw_fps(output_image, last_inference_time, cv::Point(0, 0), 0.5, 2,
             cv::Scalar(0, 0, 0), cv::Scalar(180, 180, 180), true);

    if (en_timing)
        total_postprocess_time +=
            ((rc_nanos_monotonic_time() - start_time) / 1000000.);

    return true;
}