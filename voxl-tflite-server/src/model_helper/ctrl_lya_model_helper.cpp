#include "model_helper/ctrl_lya_model_helper.h"
#include <cmath>
#include <cstring>
#include <unistd.h>

// main_running is declared in modal_start_stop.h (included transitively via modal_pipe.h)
extern volatile int main_running;

// ---- static constants -------------------------------------------------------

// Position transform: p_net = T_R_ * p_vio + T_t_
// Replace with calibrated extrinsics when VIO and network frames differ.
const float CtrlLyaModelHelper::T_R_[9] = {
    1.f, 0.f, 0.f,
    0.f, 1.f, 0.f,
    0.f, 0.f, 1.f,
};
const float CtrlLyaModelHelper::T_t_[3] = {0.f, 0.f, 0.f};

// Target pose used by the Lyapunov function: [x, y, z, yaw, pitch, roll]
const float CtrlLyaModelHelper::TARGET_POSE_[6] = {0.f, -4.f, 0.f, 1.57f, 0.f, 0.f};

// ---- VIO thread entry -------------------------------------------------------
static void *vio_thread_entry(void *arg)
{
    static_cast<CtrlLyaModelHelper *>(arg)->vio_reader_loop();
    return nullptr;
}

// ---- constructor ------------------------------------------------------------
CtrlLyaModelHelper::CtrlLyaModelHelper(char *model_file,
                                        char *labels_file,
                                        DelegateOpt delegate_choice,
                                        bool _en_debug,
                                        bool _en_timing,
                                        NormalizationType _do_normalize)
    : ModelHelper(model_file, labels_file, delegate_choice,
                  _en_debug, _en_timing, _do_normalize),
      pose_valid_(false)
{
    memset(latest_pose_, 0, sizeof(latest_pose_));
    find_tensor_indices();

    if (pthread_create(&vio_thread_, nullptr, vio_thread_entry, this) != 0)
        fprintf(stderr, "ctrl_lya: failed to start VIO reader thread\n");
}

CtrlLyaModelHelper::~CtrlLyaModelHelper()
{
    pthread_join(vio_thread_, nullptr);
}

// ---- tensor index discovery -------------------------------------------------
void CtrlLyaModelHelper::find_tensor_indices()
{
    img_input_idx_     = -1;
    pose_input_idx_    = -1;
    target_input_idx_  = -1;
    action_output_idx_ = -1;
    v_output_idx_      = -1;

    // inputs: 4-D → image; 2-D named "pose"/"target" → respective tensor
    for (int idx : interpreter->inputs()) {
        TfLiteIntArray *d = interpreter->tensor(idx)->dims;
        if (d->size == 4) {
            img_input_idx_ = idx;
            continue;
        }
        const char *name = interpreter->tensor(idx)->name;
        if (name && strstr(name, "target"))
            target_input_idx_ = idx;
        else if (name && strstr(name, "pose"))
            pose_input_idx_ = idx;
        else if (pose_input_idx_ < 0)
            pose_input_idx_ = idx;   // positional fallback: first 2-D = pose
        else if (target_input_idx_ < 0)
            target_input_idx_ = idx; // positional fallback: second 2-D = target
    }

    // outputs: shape[-1]==6 → action; other → V
    for (int idx : interpreter->outputs()) {
        TfLiteIntArray *d = interpreter->tensor(idx)->dims;
        if (d->data[d->size - 1] == 6)
            action_output_idx_ = idx;
        else
            v_output_idx_ = idx;
    }

    if (en_debug) {
        printf("ctrl_lya: img_in=%d  pose_in=%d  target_in=%d  "
               "action_out=%d  V_out=%d\n",
               img_input_idx_, pose_input_idx_, target_input_idx_,
               action_output_idx_, v_output_idx_);
    }
}

// ---- run_inference override -------------------------------------------------
bool CtrlLyaModelHelper::run_inference(cv::Mat &preprocessed_image,
                                        double *last_inference_time)
{
    uint64_t t0 = rc_nanos_monotonic_time();

    // fill image tensor (NHWC float32 in [0,1])
    // Explicit bilinear resize to 320x240 — base preprocess uses mcv_resize
    // for some camera formats, this ensures consistent interpolation.
    if (img_input_idx_ >= 0) {
        cv::Mat resized;
        cv::resize(preprocessed_image, resized,
                   cv::Size(320, 240), 0, 0, cv::INTER_LINEAR);

        float *dst = interpreter->typed_tensor<float>(img_input_idx_);
        const int row_elems = resized.cols * resized.channels();
        for (int row = 0; row < resized.rows; row++) {
            const uchar *src = resized.ptr(row);
            for (int i = 0; i < row_elems; i++)
                dst[i] = src[i] / 255.0f;
            dst += row_elems;
        }
    }

    // fill pose tensor with latest VIO pose
    if (pose_input_idx_ >= 0) {
        std::lock_guard<std::mutex> lock(pose_mutex_);
        float *dst = interpreter->typed_tensor<float>(pose_input_idx_);
        memcpy(dst, latest_pose_, 6 * sizeof(float));
    }

    // fill target tensor with fixed target pose
    if (target_input_idx_ >= 0) {
        float *dst = interpreter->typed_tensor<float>(target_input_idx_);
        memcpy(dst, TARGET_POSE_, 6 * sizeof(float));
    }

    if (interpreter->Invoke() != kTfLiteOk) {
        fprintf(stderr, "ctrl_lya: Invoke() failed\n");
        return false;
    }

    uint64_t t1 = rc_nanos_monotonic_time();
    double dt_ms = (double)(t1 - t0) / 1e6;
    if (en_timing) total_inference_time += (float)dt_ms;
    if (last_inference_time) *last_inference_time = dt_ms;

    return true;
}

// ---- postprocess (no-op for this model) -------------------------------------
bool CtrlLyaModelHelper::postprocess(cv::Mat &, double, void *)
{
    return true;
}

// ---- worker: read outputs and publish to pipe -------------------------------
bool CtrlLyaModelHelper::worker(cv::Mat &,
                                 double last_inference_time,
                                 camera_image_metadata_t metadata,
                                 void *)
{
    CtrlLyaMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.magic[0] = 'C';
    msg.magic[1] = 'L';
    msg.magic[2] = 'Y';
    msg.magic[3] = 'A';
    msg.timestamp_ns = (uint64_t)metadata.timestamp_ns;

    if (action_output_idx_ >= 0) {
        const float *a = interpreter->typed_tensor<float>(action_output_idx_);
        memcpy(msg.action, a, 6 * sizeof(float));
    }

    if (v_output_idx_ >= 0)
        msg.V = *interpreter->typed_tensor<float>(v_output_idx_);

    if (en_debug) {
        printf("ctrl_lya: V=%.4f  action=[%.3f %.3f %.3f %.3f %.3f %.3f]"
               "  dt=%.2fms\n",
               msg.V,
               msg.action[0], msg.action[1], msg.action[2],
               msg.action[3], msg.action[4], msg.action[5],
               last_inference_time);
    }

    pipe_server_write(DETECTION_CH, &msg, sizeof(msg));
    return true;
}

// ---- VIO reader loop --------------------------------------------------------
void CtrlLyaModelHelper::vio_reader_loop()
{
    int ch = pipe_client_get_next_available_channel();
    if (ch < 0) {
        fprintf(stderr, "ctrl_lya: no pipe channel available for VIO\n");
        return;
    }

    if (pipe_client_open(ch, CTRL_LYA_VIO_PIPE, "voxl-tflite-server",
                         EN_PIPE_CLIENT_AUTO_RECONNECT,
                         1024 * 1024) != 0) {
        fprintf(stderr, "ctrl_lya: failed to open VIO pipe %s\n",
                CTRL_LYA_VIO_PIPE);
        return;
    }

    int fd = pipe_client_get_fd(ch);
    if (fd < 0) {
        fprintf(stderr, "ctrl_lya: invalid fd for VIO pipe\n");
        return;
    }

    uint8_t acc[4096];
    size_t  acc_len = 0;

    // main() sets main_running=1 after create_model_helper() returns,
    // so spin here until the flag is up before entering the read loop.
    while (!main_running) usleep(10000);

    while (main_running) {
        ssize_t n = read(fd, acc + acc_len, sizeof(acc) - acc_len);
        if (n <= 0) { usleep(1000); continue; }
        acc_len += (size_t)n;

        size_t i = 0;
        while (acc_len - i >= CTRL_LYA_FRAME_SIZE) {
            // slide forward until magic is found
            if (!(acc[i+0]=='L' && acc[i+1]=='X' &&
                  acc[i+2]=='O' && acc[i+3]=='V')) {
                i++;
                continue;
            }

            pose6dof_wire_t frame;
            memcpy(&frame, acc + i, CTRL_LYA_FRAME_SIZE);
            i += CTRL_LYA_FRAME_SIZE;

            // ZYX Euler angles (radians) from row-major rotation matrix R
            double r20 = frame.R[6], r21 = frame.R[7], r22 = frame.R[8];
            double r10 = frame.R[3], r00 = frame.R[0];
            double pitch = -asin(fmax(-1.0, fmin(1.0, r20)));
            double roll  = atan2(r21, r22);
            double yaw   = atan2(r10, r00);

            // apply position transform: p_net = T_R_ * p_vio + T_t_
            float px = T_R_[0]*frame.T[0] + T_R_[1]*frame.T[1] + T_R_[2]*frame.T[2] + T_t_[0];
            float py = T_R_[3]*frame.T[0] + T_R_[4]*frame.T[1] + T_R_[5]*frame.T[2] + T_t_[1];
            float pz = T_R_[6]*frame.T[0] + T_R_[7]*frame.T[1] + T_R_[8]*frame.T[2] + T_t_[2];

            {
                std::lock_guard<std::mutex> lock(pose_mutex_);
                latest_pose_[0] = px;
                latest_pose_[1] = py;
                latest_pose_[2] = pz;
                latest_pose_[3] = (float)yaw;
                latest_pose_[4] = (float)pitch;
                latest_pose_[5] = (float)roll;
                pose_valid_ = true;
            }
        }

        // compact the accumulator buffer
        if (i > 0) {
            memmove(acc, acc + i, acc_len - i);
            acc_len -= i;
        }
    }
}
