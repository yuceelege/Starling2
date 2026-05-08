#ifndef CTRL_LYA_MODEL_HELPER_H
#define CTRL_LYA_MODEL_HELPER_H

#include "model_helper/model_helper.h"
#include <pthread.h>
#include <mutex>
#include <stdint.h>

// MPA pipe path for VIO body-wrt-local pose
#define CTRL_LYA_VIO_PIPE   "/run/mpa/vvhub_body_wrt_local"
#define CTRL_LYA_FRAME_SIZE  84   // 4B magic + 8B ts + 18*4B floats

// On-wire layout of each pose6dof frame
#pragma pack(push, 1)
struct pose6dof_wire_t {
    uint8_t  magic[4];       // 'L','X','O','V'
    int64_t  timestamp_ns;
    float    T[3];           // position (m) body w.r.t. local frame
    float    R[9];           // rotation matrix (row-major) body->local
    float    v[3];           // linear velocity (m/s)
    float    w[3];           // angular rate (rad/s)
};
#pragma pack(pop)

// Published output: controller action + Lyapunov value
#pragma pack(push, 1)
struct CtrlLyaMsg {
    uint8_t  magic[4];       // 'C','L','Y','A'
    float    action[6];      // [vx, vy, vz, vyaw, vpitch, vroll]
    float    V;              // Lyapunov value
    uint64_t timestamp_ns;
};
#pragma pack(pop)

#define CTRL_LYA_MSG_SIZE  40   // 4 + 6*4 + 4 + 8

class CtrlLyaModelHelper : public ModelHelper {
public:
    CtrlLyaModelHelper(char *model_file,
                       char *labels_file,
                       DelegateOpt delegate_choice,
                       bool _en_debug,
                       bool _en_timing,
                       NormalizationType _do_normalize);

    ~CtrlLyaModelHelper() override;

    // Fills all three input tensors (image + pose + target) then invokes
    bool run_inference(cv::Mat &preprocessed_image,
                       double *last_inference_time) override;

    bool worker(cv::Mat &output_image,
                double last_inference_time,
                camera_image_metadata_t metadata,
                void *input_params) override;

    bool postprocess(cv::Mat &output_image,
                     double last_inference_time,
                     void *input_params) override;

    // Entry point for the VIO reader thread (public so the static thunk works)
    void vio_reader_loop();

private:
    // Coordinate transform applied to VIO position:
    //   p_net = T_R_ * p_vio + T_t_
    // Defaults to identity (no transform). Replace with calibrated values.
    static const float T_R_[9];
    static const float T_t_[3];

    // Fixed target pose: [x, y, z, yaw, pitch, roll]
    static const float TARGET_POSE_[6];

    // Latest transformed pose received from VIO pipe
    std::mutex pose_mutex_;
    float      latest_pose_[6];
    bool       pose_valid_;

    // Cached tensor indices resolved once at construction
    int img_input_idx_;
    int pose_input_idx_;
    int target_input_idx_;
    int action_output_idx_;
    int v_output_idx_;

    pthread_t vio_thread_;

    void find_tensor_indices();
};

#endif // CTRL_LYA_MODEL_HELPER_H
