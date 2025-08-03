#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <modal_pipe_client.h>
#include <modal_pipe_common.h>

typedef struct {
    float vx;
    float vy;
    float vz;
    float yaw;
    uint64_t timestamp;
} ControlMsg;

int main() {
    int ch = pipe_client_get_next_available_channel();
    if (ch < 0) {
        return 1;
    }

    if (pipe_client_open(ch,
                         "/run/mpa/zeroshot_tflite_data",
                         "mpa_reader",
                         0,
                         0) != 0) {
        return 1;
    }

    int fd = pipe_client_get_fd(ch);
    if (fd < 0) {
        return 1;
    }

    ControlMsg msg;
    while (1) {
        ssize_t r = read(fd, &msg, sizeof(msg));
        if (r == sizeof(msg)) {
            fwrite(&msg, sizeof(msg), 1, stdout);
            fflush(stdout);
        } else {
            usleep(1000);
        }
    }

    return 0;
}
