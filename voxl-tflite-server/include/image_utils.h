#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H


#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/types_c.h>

cv::Scalar get_color_from_id(int32_t id);

void draw_text(cv::Mat &mat, const std::string &text, cv::Point pos,
               double font_scale, int32_t thickness,
               cv::Scalar color_front, cv::Scalar color_back,
               bool is_text_on_rect);

void draw_fps(cv::Mat &mat, double time_inference, cv::Point pos,
                     double font_scale, int32_t thickness,
                     cv::Scalar color_front, cv::Scalar color_back,
                     bool is_text_on_rect);

#endif