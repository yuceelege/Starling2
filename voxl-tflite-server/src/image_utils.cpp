#include "image_utils.h"

// gets some nice randomly generated colors for ids
// when grayscale, will be shades of gray
cv::Scalar get_color_from_id(int32_t id)
{
    static constexpr int32_t kMaxNum = 100;
    static std::vector<cv::Scalar> color_list;
    if (color_list.empty())
    {
        std::srand(123);
        for (int32_t i = 0; i < kMaxNum; i++)
        {
            color_list.push_back(cv::Scalar(
                std::rand() % 255, std::rand() % 255, std::rand() % 255));
        }
    }
    return color_list[id % kMaxNum];
}

void draw_text(cv::Mat &mat, const std::string &text, cv::Point pos,
                      double font_scale, int32_t thickness,
                      cv::Scalar color_front, cv::Scalar color_back,
                      bool is_text_on_rect)
{
    int32_t baseline = 0;
    cv::Size textSize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX,
                                        font_scale, thickness, &baseline);
    baseline += thickness;
    pos.y += textSize.height;
    if (is_text_on_rect)
    {
        cv::rectangle(mat, pos + cv::Point(0, baseline),
                      pos + cv::Point(textSize.width, -textSize.height),
                      color_back, -1);
        cv::putText(mat, text, pos, cv::FONT_HERSHEY_SIMPLEX, font_scale,
                    color_front, thickness);
    }
    else
    {
        cv::putText(mat, text, pos, cv::FONT_HERSHEY_SIMPLEX, font_scale,
                    color_back, thickness * 3);
        cv::putText(mat, text, pos, cv::FONT_HERSHEY_SIMPLEX, font_scale,
                    color_front, thickness);
    }
}

void draw_fps(cv::Mat &mat, double time_inference, cv::Point pos,
                     double font_scale, int32_t thickness,
                     cv::Scalar color_front, cv::Scalar color_back,
                     bool is_text_on_rect)
{
    char text[64];
    static auto time_previous = std::chrono::steady_clock::now();
    auto time_now = std::chrono::steady_clock::now();
    double fps = 1e9 / (time_now - time_previous).count();
    time_previous = time_now;
    snprintf(text, sizeof(text), "FPS: %.1f, Inference: %.1f [ms]", fps,
             time_inference);
    draw_text(mat, text, cv::Point(0, 0), 0.5, 2, color_front, color_back,
              true);
}