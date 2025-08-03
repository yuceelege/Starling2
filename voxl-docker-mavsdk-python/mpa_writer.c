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
                         "/run/mpa/control_out",
                         "mpa_writer",
                         1,
                         0) != 0) {
        return 1;
    }

    int fd = pipe_client_get_fd(ch);
    if (fd < 0) {
        return 1;
    }

    // Read control messages from stdin and write to pipe
    ControlMsg msg;
    while (1) {
        ssize_t r = read(STDIN_FILENO, &msg, sizeof(msg));
        if (r == sizeof(msg)) {
            write(fd, &msg, sizeof(msg));
        } else {
            usleep(1000);
        }
    }

    return 0;
} 