// build:  gcc -O2 -o mpa_reader mpa_reader.c -lmodal_pipe -lm
// usage:  ./mpa_reader
// reads CtrlLyaMsg packets from /run/mpa/tflite_data/ and streams
// raw binary to stdout for Python consumers.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <modal_pipe_client.h>

#define PIPE_PATH   "/run/mpa/ctrl_lya_tflite_data/"
#define CLIENT_NAME "ctrl_lya_reader"

// Must match CtrlLyaMsg in ctrl_lya_model_helper.h
#define FRAME_SIZE  ((int)sizeof(CtrlLyaMsg))   // 4 + 24 + 4 + 8 = 40 bytes

#pragma pack(push, 1)
typedef struct {
    uint8_t  magic[4];   // 'C','L','Y','A'
    float    action[6];  // [vx, vy, vz, vyaw, vpitch, vroll]
    float    V;
    uint64_t timestamp_ns;
} CtrlLyaMsg;
#pragma pack(pop)

static volatile int running = 1;
static void _sig_handler(int sig) { (void)sig; running = 0; }

static void _connect_cb(int ch, void *ctx)
{
    (void)ch; (void)ctx;
    fprintf(stderr, "ctrl_lya pipe connected\n");
}

static void _disconnect_cb(int ch, void *ctx)
{
    (void)ch; (void)ctx;
    fprintf(stderr, "ctrl_lya pipe disconnected\n");
}

int main(void)
{
    signal(SIGINT,  _sig_handler);
    signal(SIGTERM, _sig_handler);

    int ch = pipe_client_get_next_available_channel();
    if (ch < 0) {
        fprintf(stderr, "mpa_reader: no pipe channel available\n");
        return 1;
    }

    pipe_client_set_connect_cb(ch, _connect_cb, NULL);
    pipe_client_set_disconnect_cb(ch, _disconnect_cb, NULL);

    if (pipe_client_open(ch, PIPE_PATH, CLIENT_NAME,
                         EN_PIPE_CLIENT_AUTO_RECONNECT,
                         1024 * 1024) != 0) {
        fprintf(stderr, "mpa_reader: failed to open %s\n", PIPE_PATH);
        return 1;
    }

    int fd = pipe_client_get_fd(ch);
    if (fd < 0) {
        fprintf(stderr, "mpa_reader: invalid fd\n");
        return 1;
    }

    uint8_t acc[4096];
    size_t  acc_len = 0;

    while (running) {
        ssize_t n = read(fd, acc + acc_len, sizeof(acc) - acc_len);
        if (n <= 0) { usleep(1000); continue; }
        acc_len += (size_t)n;

        size_t i = 0;
        while (acc_len - i >= FRAME_SIZE) {
            // scan for 'CLYA' magic
            if (!(acc[i+0]=='C' && acc[i+1]=='L' &&
                  acc[i+2]=='Y' && acc[i+3]=='A')) {
                i++;
                continue;
            }

            CtrlLyaMsg msg;
            memcpy(&msg, acc + i, FRAME_SIZE);
            i += FRAME_SIZE;

            fwrite(&msg, sizeof(msg), 1, stdout);
            fflush(stdout);
        }

        if (i > 0) {
            memmove(acc, acc + i, acc_len - i);
            acc_len -= i;
        }

        if (acc_len > sizeof(acc) - FRAME_SIZE)
            acc_len = 0;
    }

    pipe_client_close_all();
    return 0;
}
