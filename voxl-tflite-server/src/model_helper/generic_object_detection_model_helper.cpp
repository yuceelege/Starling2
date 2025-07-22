#include "model_helper/generic_object_detection_model_helper.h"
#include "tensor_data.h"
#include "image_utils.h"

GenericObjectDetectionModelHelper::GenericObjectDetectionModelHelper(char *model_file, char *labels_file,
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

bool GenericObjectDetectionModelHelper::worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params)
{
    if (!postprocess(output_image, last_inference_time, input_params))
        return false;

    if (!detections_vector.empty())
    {
        pipe_server_write(DETECTION_CH,
            (char *)detections_vector.data(),
            sizeof(ai_detection_t) * detections_vector.size());
    }
    metadata.timestamp_ns = rc_nanos_monotonic_time();
    pipe_server_write_camera_frame(IMAGE_CH, metadata, (char *)output_image.data);

    return true;
}

bool GenericObjectDetectionModelHelper::postprocess(cv::Mat &output_image, double last_inference_time, void *input_params)
{
    // declare a temp vector and then swap with the class member
    // avoids the vector clearing overhead
    std::vector<ai_detection_t> temp_vector;

    start_time = rc_nanos_monotonic_time();

    // https://www.tensorflow.org/lite/models/object_detection/overview#starter_model
    TfLiteTensor *output_locations =
        interpreter->tensor(interpreter->outputs()[0]);
    TfLiteTensor *output_classes =
        interpreter->tensor(interpreter->outputs()[1]);
    TfLiteTensor *output_scores =
        interpreter->tensor(interpreter->outputs()[2]);
    TfLiteTensor *output_detections =
        interpreter->tensor(interpreter->outputs()[3]);

    const float *detected_locations = TensorData<float>(output_locations, 0);
    const float *detected_classes = TensorData<float>(output_classes, 0);
    const float *detected_scores = TensorData<float>(output_scores, 0);

    const int detected_numclasses =
        (int)(*TensorData<float>(output_detections, 0));

    for (int i = 0; i < detected_numclasses; i++)
    {
        const float score = detected_scores[i];
        // scale bboxes back to input resolution
        const int top = detected_locations[4 * i + 0] * input_height;
        const int left = detected_locations[4 * i + 1] * input_width;
        const int bottom = detected_locations[4 * i + 2] * input_height;
        const int right = detected_locations[4 * i + 3] * input_width;

        // Check for object detection confidence of 60% or more
        if (score > 0.6f)
        {
            if (en_debug)
            {
                printf("Detected: %s, Confidence: %6.2f\n",
                       labels[detected_classes[i]].c_str(), (double)score);
            }
            int height = bottom - top;
            int width = right - left;

            cv::Rect rect(left, top, width, height);
            cv::Point pt(left, top - 10);

            cv::rectangle(output_image, rect,
                          get_color_from_id(detected_classes[i]), 2);
            cv::putText(output_image, labels[detected_classes[i]], pt,
                        cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0), 2);

            // setup ai detection for this detection
            ai_detection_t curr_detection;
            curr_detection.magic_number = AI_DETECTION_MAGIC_NUMBER;
            curr_detection.timestamp_ns = rc_nanos_monotonic_time();
            curr_detection.class_id = detected_classes[i];
            curr_detection.frame_id = num_frames_processed;

            std::string class_holder = labels[detected_classes[i]].substr(
                labels[detected_classes[i]].find(" ") + 1);
            class_holder.erase(
                remove_if(class_holder.begin(), class_holder.end(), isspace),
                class_holder.end());
            strcpy(curr_detection.class_name, class_holder.c_str());

            strcpy(curr_detection.cam, cam_name.c_str());
            curr_detection.class_confidence = score;
            curr_detection.detection_confidence =
                -1; // UNKNOWN for ssd model architecture
            curr_detection.x_min = left;
            curr_detection.y_min = top;
            curr_detection.x_max = right;
            curr_detection.y_max = bottom;

            // fill the vector
            temp_vector.push_back(curr_detection);
        }
    }

    detections_vector.swap(temp_vector);
    
    draw_fps(output_image, last_inference_time, cv::Point(0, 0), 0.5, 2,
             cv::Scalar(0, 0, 0), cv::Scalar(180, 180, 180), true);

    if (en_timing)
        total_postprocess_time +=
            ((rc_nanos_monotonic_time() - start_time) / 1000000.);

    return true;
}
