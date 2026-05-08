// build:  gcc -O2 -o read_local_pose read_local_pose.c -lmodal_pipe -lm
// usage:  ./read_local_pose
// notes:  parses /run/mpa/vvhub_body_wrt_local and prints x y z and RPY

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <modal_pipe_client.h>

#define PIPE_PATH "/run/mpa/vvhub_body_wrt_local"
#define CLIENT_NAME "mpa_reader"
#define FRAME_MAGIC_0 'L'  // 'L''X''O''V' == 0x4C 58 4F 56
#define FRAME_MAGIC_1 'X'
#define FRAME_MAGIC_2 'O'
#define FRAME_MAGIC_3 'V'

// On-wire layout: 4B magic + 8B ts + 18*4B floats = 84 bytes
#define FRAME_SIZE 84

#pragma pack(push, 1)
typedef struct {
   uint8_t  magic[4];   // 'L','X','O','V'
   int64_t  timestamp_ns;
   float    T[3];       // position (m) body w.r.t. local
   float    R[9];       // rotation matrix (row-major) body->local
   float    v[3];       // linear velocity (m/s) in local
   float    w[3];       // angular rate (rad/s) in body
} pose6dof_wire_t;
#pragma pack(pop)

// Output structure for Python (like GateXyzMsg)
typedef struct {
   float x;
   float y;
   float z;
   float yaw;
   float vx;    // linear velocity x component (m/s)
   float vy;    // linear velocity y component (m/s)
   float vz;    // linear velocity z component (m/s)
} VioPoseMsg;

static void rpy_from_R(const float R[9], double *roll_deg, double *pitch_deg, double *yaw_deg)
{
   // ZYX euler from row-major rotation matrix
   double r00 = R[0], r01 = R[1], r02 = R[2];
   double r10 = R[3], r11 = R[4], r12 = R[5];
   double r20 = R[6], r21 = R[7], r22 = R[8];

   double pitch = -asin(fmax(-1.0, fmin(1.0, r20)));
   double roll  = atan2(r21, r22);
   double yaw   = atan2(r10, r00);

   const double RAD2DEG = 180.0/M_PI;
   *roll_deg  = roll  * RAD2DEG;
   *pitch_deg = pitch * RAD2DEG;
   *yaw_deg   = yaw   * RAD2DEG;
}

int main(void)
{
   int ch = pipe_client_get_next_available_channel();
   if (ch < 0) {
       return 1;
   }

   if (pipe_client_open(ch, PIPE_PATH, CLIENT_NAME,
                        EN_PIPE_CLIENT_AUTO_RECONNECT,
                        1024 * 1024) != 0) {
       return 1;
   }

   int fd = pipe_client_get_fd(ch);
   if (fd < 0) {
       return 1;
   }

   // Small ring buffer to safely handle partial reads or multiple frames per read
   uint8_t acc[4096];
   size_t acc_len = 0;

   // Read one frame and output x y z yaw as raw binary
   while (1) {
       // read some bytes
       ssize_t n = read(fd, acc + acc_len, sizeof(acc) - acc_len);
       if (n < 0) { usleep(1000); continue; }
       if (n == 0) break;
       acc_len += (size_t)n;

       // find frames inside the buffer
       size_t i = 0;
       while (acc_len - i >= FRAME_SIZE) {
           // ensure magic alignment; if not aligned, slide forward
           if (!(acc[i+0]==FRAME_MAGIC_0 && acc[i+1]==FRAME_MAGIC_1 &&
                 acc[i+2]==FRAME_MAGIC_2 && acc[i+3]==FRAME_MAGIC_3))
           {
               i++; // skip until we hit magic
               continue;
           }

           // We have a full frame?
           if (acc_len - i < FRAME_SIZE) break;

           pose6dof_wire_t pkt;
           memcpy(&pkt, acc + i, FRAME_SIZE);

           // decode to get yaw
           double roll, pitch, yaw;
           rpy_from_R(pkt.R, &roll, &pitch, &yaw);

           // Create output message (like GateXyzMsg)
           VioPoseMsg msg;
           msg.x = pkt.T[0];
           msg.y = pkt.T[1];
           msg.z = pkt.T[2];
           msg.yaw = (float)yaw;
           msg.vx = pkt.v[0];    // linear velocity x component
           msg.vy = pkt.v[1];    // linear velocity y component
           msg.vz = pkt.v[2];    // linear velocity z component

           // Output raw binary to stdout (like your working code)
           fwrite(&msg, sizeof(msg), 1, stdout);
           fflush(stdout);
           
           return 0;  // Exit after first successful frame
       }

       // compact remaining bytes to front
       if (i > 0) {
           memmove(acc, acc + i, acc_len - i);
           acc_len -= i;
       }

       // If buffer nearly full without finding magic, drop it
       if (acc_len > sizeof(acc) - FRAME_SIZE) {
           acc_len = 0;
       }
   }

   return 1;  // No frame found
}
