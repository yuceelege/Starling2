#include "model_helper/yolov8_model_helper.h"
#include "tensor_data.h"
#include "image_utils.h"

YoloV8ModelHelper::YoloV8ModelHelper(char *model_file, char *labels_file,
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

bool YoloV8ModelHelper::worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params)
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

bool YoloV8ModelHelper::postprocess(cv::Mat &output_image, double last_inference_time, void *input_params)
{

    // declare a temp vector and then swap with the class member
    // avoids the vector clearing overhead
    std::vector<ai_detection_t> temp_vector;

    start_time = rc_nanos_monotonic_time();

    if (labels.empty())
    {
        if (ReadLabelsFile(labels_location, &labels, &label_count) !=
            kTfLiteOk)
        {
            fprintf(stderr, "ERROR: Unable to read labels file\n");
            return false;
        }
    }

    // Assuming the preprocessing and inference steps were written correctly
    TfLiteTensor *output_tensor = interpreter->tensor(interpreter->outputs()[0]);
    int rows, dimensions;
    const auto &output_shape = output_tensor->dims;

    rows = output_shape->data[2];
    dimensions = output_shape->data[1];
    int output_index = interpreter->outputs()[0];

    // Not using the TensorData function since the output is wrapped in a CV Mat and then raw data
    // is extracted out as a float* directly. Index calculations are performed manually

    float *data = interpreter->typed_tensor<float>(output_index);
    // Unsure of the overhead wrapping with a CV mat might induce, you index calculations can be done
    // even more manualy for transposing and reshapes, but I dont think the benefits will be tangible

    cv::Mat temp(dimensions, rows, CV_32F, data); // Wrap with a cv mat to perform tensor reshapes and
                                                  // and transpose options
    cv::transpose(temp, temp);
    float *new_data = (float *)temp.data;

    // would rather not use this since it is storing redundant info
    // std::vector<b_box> bbox_list;
    
    std::vector<cv::Rect> boxes;
    std::vector<int> class_ids;
    std::vector<float> confidences;

    for (int i = 0; i < rows; i++)
    {
        float *classes_scores = new_data + 4;

        cv::Mat scores(1, label_count, CV_32FC1, classes_scores);
        cv::Point class_id;
        double maxClassScore;

        cv::minMaxLoc(scores, 0, &maxClassScore, 0, &class_id);

        if (maxClassScore > model_score_threshold)
        {
            confidences.push_back(maxClassScore);
            class_ids.push_back(class_id.x);

            float xc = new_data[0];
            float yc = new_data[1];
            float w = new_data[2];
            float h = new_data[3];

            int left = int((xc - 0.5 * w) * input_width);
            int top = int((yc - 0.5 * h) * input_height);

            int width = int(w * input_width);
            int height = int(h * input_height);

            boxes.push_back(cv::Rect(left, top, width, height));
        }
        new_data += dimensions;
    }

    std::vector<int> nms_result;
    cv::dnn::NMSBoxes(boxes, confidences, model_confidence_threshold, model_nms_threshold, nms_result);

    for (unsigned long i = 0; i < nms_result.size(); ++i)
    {

        int idx = nms_result[i];

        cv::putText(output_image, labels[class_ids[idx]],
                    cv::Point(boxes[idx].x, boxes[idx].y), cv::FONT_HERSHEY_SIMPLEX, 0.8,
                    cv::Scalar(0), 2);
        cv::rectangle(output_image, cv::Rect(boxes[idx].x, boxes[idx].y, boxes[idx].x + boxes[idx].width, boxes[idx].y + boxes[idx].height),
                      get_color_from_id(class_ids[idx]), 2);

        ai_detection_t curr_detection;
        curr_detection.magic_number = AI_DETECTION_MAGIC_NUMBER;
        curr_detection.timestamp_ns = rc_nanos_monotonic_time();
        curr_detection.class_id = class_ids[idx];
        curr_detection.class_confidence = confidences[idx];
        curr_detection.frame_id = num_frames_processed;
        curr_detection.detection_confidence = -1.0; // detection confidence is not a thing for yolov8

        std::string class_holder = labels[class_ids[idx]].substr(
            labels[class_ids[idx]].find(" ") + 1);
        class_holder.erase(
            remove_if(class_holder.begin(), class_holder.end(), isspace),
            class_holder.end());
        strcpy(curr_detection.class_name, class_holder.c_str());

        strcpy(curr_detection.cam, cam_name.c_str());

        curr_detection.x_min = boxes[idx].x;
        curr_detection.y_min = boxes[idx].y;
        curr_detection.x_max = boxes[idx].x + boxes[idx].width;
        curr_detection.y_max = boxes[idx].y + boxes[idx].height;

        temp_vector.push_back(curr_detection);
    }

    detections_vector.swap(temp_vector);
    draw_fps(output_image, last_inference_time, cv::Point(0, 0), 0.5, 2,
             cv::Scalar(0, 0, 0), cv::Scalar(180, 180, 180), true);

    return true;
}

bool YoloV8ModelHelper::run_inference(cv::Mat &preprocessed_image,
                                      double *last_inference_time)
{

    // GPU delegate doesn't seem to work for yolov8 for whatever reason

    start_time = rc_nanos_monotonic_time();
    int input = interpreter->inputs()[0];
    float *input_data = interpreter->typed_tensor<float>(input);
    std::memcpy(input_data, preprocessed_image.data, sizeof(float) * 640 * 640 * 3);

    if (interpreter->Invoke() != kTfLiteOk)
    {
        std::cerr << "Failed to invoke tflite interpreter." << std::endl;
        return false;
    }
    int64_t end_time = rc_nanos_monotonic_time();

    if (en_timing) {
        total_inference_time += ((end_time - start_time) / 1000000.);
    }
    if (last_inference_time != nullptr) {
        *last_inference_time = ((double)(end_time - start_time) / 1000000.);
    }

    return true;
}

bool YoloV8ModelHelper::preprocess(camera_image_metadata_t &meta,
                                 char *frame, std::shared_ptr<cv::Mat> preprocessed_image,
                                 std::shared_ptr<cv::Mat> output_image)
{
    start_time = rc_nanos_monotonic_time();
    num_frames_processed++;

    if (num_frames_processed == 1)
    {
        mcv_init_resize_map(meta.width, meta.height, model_width, model_height, &map);

        input_height = meta.height;
        input_width = meta.width;

        if (meta.format == IMAGE_FORMAT_RAW8)
        {
            resize_output = (uint8_t *)malloc(model_height * model_width * sizeof(uint8_t));
        }
        else
        {
            resize_output = (uint8_t *)malloc(model_height * model_width * sizeof(uint8_t) * 3);
        }
        return false;
    }

    switch (meta.format)
    {
    case IMAGE_FORMAT_STEREO_NV12:
        meta.format = IMAGE_FORMAT_NV12;
    case IMAGE_FORMAT_NV12:
    {
        cv::Mat yuv(input_height + input_height / 2, input_width, CV_8UC1, (uchar *)frame);
        cv::cvtColor(yuv, *output_image, CV_YUV2RGB_NV12);
        mcv_resize_8uc3_image(output_image->data, resize_output, &map);
        cv::Mat holder(model_height, model_width, CV_8UC3, (uchar *)resize_output);

        *preprocessed_image = holder;
        meta.format = IMAGE_FORMAT_RGB;
        meta.size_bytes = (meta.height * meta.width * 3);
        meta.stride = (meta.width * 3);
    }
    break;

    case IMAGE_FORMAT_YUV422:
    {
        cv::Mat yuv(input_height, input_width, CV_8UC2, (uchar *)frame);
        cv::cvtColor(yuv, *output_image, CV_YUV2RGB_YUYV);
        mcv_resize_8uc3_image(output_image->data, resize_output, &map);
        cv::Mat holder(model_height, model_width, CV_8UC3, (uchar *)resize_output);

        *preprocessed_image = holder;
        meta.format = IMAGE_FORMAT_RGB;
        meta.size_bytes = (meta.height * meta.width * 3);
        meta.stride = (meta.width * 3);
    }
    break;

    case IMAGE_FORMAT_STEREO_NV21:
        meta.format = IMAGE_FORMAT_NV21;
    case IMAGE_FORMAT_NV21:
    {
        cv::Mat yuv(input_height + input_height / 2, input_width, CV_8UC1, (uchar *)frame);
        cv::cvtColor(yuv, *output_image, CV_YUV2RGB_NV21);
        mcv_resize_8uc3_image(output_image->data, resize_output, &map);
        cv::Mat holder(model_height, model_width, CV_8UC3, (uchar *)resize_output);

        *preprocessed_image = holder;
        meta.format = IMAGE_FORMAT_RGB;
        meta.size_bytes = (meta.height * meta.width * 3);
        meta.stride = (meta.width * 3);
    }
    break;

    case IMAGE_FORMAT_STEREO_RAW8:
        meta.format = IMAGE_FORMAT_RAW8;
    case IMAGE_FORMAT_RAW8:
    {
        *output_image = cv::Mat(input_height, input_width, CV_8UC1, (uchar *)frame);

        mcv_resize_image(output_image->data, resize_output, &map);

        cv::Mat holder(model_height, model_width, CV_8UC1, (uchar *)resize_output);
        cv::Mat in[] = {holder, holder, holder};
        cv::merge(in, 3, *preprocessed_image);
    }
    break;

    default:
        fprintf(stderr, "Unexpected image format %d received! Exiting now.\n", meta.format);
        return false;
    }

    // Normalize values
    (*preprocessed_image).convertTo(*preprocessed_image, CV_32FC3, 1.0 / 255.0);

    // Reshape (640, 640, 3) to (1, 640, 640, 3)
    *preprocessed_image = (*preprocessed_image).reshape(1, {1, (*preprocessed_image).rows, (*preprocessed_image).cols, (*preprocessed_image).channels()});

    if (en_timing)
        total_preprocess_time += ((rc_nanos_monotonic_time() - start_time) / 1000000.);

    return true;
}
