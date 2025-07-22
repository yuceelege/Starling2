#include "model_helper/yolov5_model_helper.h"
#include "tensor_data.h"
#include "image_utils.h"

constexpr int32_t YoloV5ModelHelper::kGridScaleList[3];

YoloV5ModelHelper::YoloV5ModelHelper(char *model_file, char *labels_file,
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

bool YoloV5ModelHelper::worker(cv::Mat &output_image, double last_inference_time, camera_image_metadata_t metadata, void *input_params)
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

bool YoloV5ModelHelper::postprocess(cv::Mat &output_image, double last_inference_time, void *input_params)
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

    // yolo has just one fat float output tensor
    TfLiteTensor *output_locations =
        interpreter->tensor(interpreter->outputs()[0]);
    float *output_tensor = TensorData<float>(output_locations, 0);

    std::vector<b_box> bbox_list;

    for (const auto &scale : kGridScaleList)
    {
        int32_t grid_w = model_width / scale;
        int32_t grid_h = model_height / scale;
        float scale_x = static_cast<float>(input_width);
        float scale_y = static_cast<float>(input_height);
        get_bbox(output_tensor, scale_x, scale_y, grid_w, grid_h, label_count,
                 bbox_list);
        output_tensor += grid_w * grid_h * kGridChannel * kElementNumOfAnchor;
    }

    std::vector<b_box> bbox_nms_list;
    nms(bbox_list, bbox_nms_list, threshold_nms_iou_, false);

    for (const auto &bbox : bbox_nms_list)
    {
        cv::putText(output_image, labels[bbox.class_id],
                    cv::Point(bbox.x, bbox.y), cv::FONT_HERSHEY_SIMPLEX, 0.8,
                    cv::Scalar(0), 2);
        cv::rectangle(output_image, cv::Rect(bbox.x, bbox.y, bbox.w, bbox.h),
                      get_color_from_id(bbox.class_id), 2);
        // setup ai detection for this detection
        ai_detection_t curr_detection;
        curr_detection.magic_number = AI_DETECTION_MAGIC_NUMBER;
        curr_detection.timestamp_ns = rc_nanos_monotonic_time();
        curr_detection.class_id = bbox.class_id;
        curr_detection.frame_id = num_frames_processed;

        strcpy(curr_detection.class_name, labels[bbox.class_id].c_str());
        strcpy(curr_detection.cam, cam_name.c_str());

        curr_detection.class_confidence = bbox.class_conf;
        curr_detection.detection_confidence = bbox.detection_conf;
        curr_detection.x_min = bbox.x;
        curr_detection.y_min = bbox.y;
        curr_detection.x_max = bbox.x + bbox.w;
        curr_detection.y_max = bbox.y + bbox.h;

        // fill the vector
        temp_vector.push_back(curr_detection);
    }

    detections_vector.swap(temp_vector);

    draw_fps(output_image, last_inference_time, cv::Point(0, 0), 0.5, 2,
             cv::Scalar(0, 0, 0), cv::Scalar(180, 180, 180), true);

    if (en_timing)
        total_postprocess_time +=
            ((rc_nanos_monotonic_time() - start_time) / 1000000.);

    return true;
}

void YoloV5ModelHelper::get_bbox(const float *data, float scale_x, float scale_y,
                                 int32_t grid_w, int32_t grid_h, int number_of_classes,
                                 std::vector<b_box> &bbox_list)
{
    int actual_loops = 0;
    int n_skipped = 0;
    int32_t index = 0;

    // assign these per call
    kNumberOfClass = number_of_classes;
    kElementNumOfAnchor =
        kNumberOfClass + 5; // x, y, w, h, bbox confidence, [class confidence]

    for (int32_t grid_y = 0; grid_y < grid_h; grid_y++)
    {
        for (int32_t grid_x = 0; grid_x < grid_w; grid_x++)
        {
            for (int32_t grid_c = 0; grid_c < kGridChannel; grid_c++)
            {
                actual_loops++;
                float box_confidence = data[index + 4];

                if (box_confidence >= threshold_box_confidence_)
                {
                    int32_t class_id = 0;
                    float confidence = 0;
                    float confidence_of_class = 0;
                    for (int32_t class_index = 0; class_index < kNumberOfClass;
                         class_index++)
                    {
                        confidence_of_class = data[index + 5 + class_index];
                        if (confidence_of_class > confidence)
                        {
                            confidence = confidence_of_class;
                            class_id = class_index;
                        }
                    }

                    if (confidence >= threshold_class_confidence_)
                    {
                        int32_t cx = static_cast<int32_t>(
                            (data[index + 0] + 0) *
                            scale_x); // no need to + grid_x
                        int32_t cy = static_cast<int32_t>(
                            (data[index + 1] + 0) *
                            scale_y); // no need to + grid_y
                        int32_t w = static_cast<int32_t>(
                            data[index + 2] * scale_x); // no need to exp
                        int32_t h = static_cast<int32_t>(
                            data[index + 3] * scale_y); // no need to exp
                        int32_t x = cx - w / 2;
                        int32_t y = cy - h / 2;
                        b_box bbox = {class_id,
                                      "",
                                      confidence_of_class,
                                      box_confidence,
                                      confidence,
                                      x,
                                      y,
                                      w,
                                      h};
                        bbox_list.push_back(bbox);
                    }
                }
                else
                    n_skipped++;
                index += kElementNumOfAnchor;
            }
        }
    }
}

void YoloV5ModelHelper::nms(std::vector<b_box> &bbox_list,
                            std::vector<b_box> &bbox_nms_list, float threshold_nms_iou,
                            bool check_class_id)
{
    std::sort(bbox_list.begin(), bbox_list.end(),
              [](b_box const &lhs, b_box const &rhs)
              {
                  if (lhs.score > rhs.score)
                      return true;
                  return false;
              });

    std::unique_ptr<bool[]> is_merged(new bool[bbox_list.size()]);
    for (size_t i = 0; i < bbox_list.size(); i++)
        is_merged[i] = false;
    for (size_t index_high_score = 0; index_high_score < bbox_list.size();
         index_high_score++)
    {
        std::vector<b_box> candidates;
        if (is_merged[index_high_score])
            continue;
        candidates.push_back(bbox_list[index_high_score]);
        for (size_t index_low_score = index_high_score + 1;
             index_low_score < bbox_list.size(); index_low_score++)
        {
            if (is_merged[index_low_score])
                continue;
            if (check_class_id && bbox_list[index_high_score].class_id !=
                                      bbox_list[index_low_score].class_id)
                continue;
            if (calc_iou(bbox_list[index_high_score],
                         bbox_list[index_low_score]) > threshold_nms_iou)
            {
                candidates.push_back(bbox_list[index_low_score]);
                is_merged[index_low_score] = true;
            }
        }
        bbox_nms_list.push_back(candidates[0]);
    }
}

float YoloV5ModelHelper::calc_iou(const b_box &obj0, const b_box &obj1)
{
    int32_t interx0 = (std::max)(obj0.x, obj1.x);
    int32_t intery0 = (std::max)(obj0.y, obj1.y);
    int32_t interx1 = (std::min)(obj0.x + obj0.w, obj1.x + obj1.w);
    int32_t intery1 = (std::min)(obj0.y + obj0.h, obj1.y + obj1.h);
    if (interx1 < interx0 || intery1 < intery0)
        return 0;

    int32_t area0 = obj0.w * obj0.h;
    int32_t area1 = obj1.w * obj1.h;
    int32_t areaInter = (interx1 - interx0) * (intery1 - intery0);
    int32_t areaSum = area0 + area1 - areaInter;

    return static_cast<float>(areaInter) / areaSum;
}
