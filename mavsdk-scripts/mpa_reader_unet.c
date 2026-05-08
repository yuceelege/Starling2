// build:  gcc -O2 -o mpa_reader mpa_reader.c -lmodal_pipe -lm
// usage:  ./mpa_reader
// notes:  reads UNet output from voxl-tflite-server pipe and extracts single channel from RGB

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <modal_pipe_client.h>

#define PIPE_PATH "/run/mpa/unet_tflite"
#define CLIENT_NAME "unet_reader"

// Camera frame metadata structure is already defined in modal_pipe_interfaces.h

// UNet output message structure (single channel)
typedef struct {
    uint64_t timestamp_ns;
    int width;
    int height;
    uint8_t mask_data[];  // Single channel mask data
} UnetOutputMsg;

// Global flag for signal handling
volatile int keep_running = 1;

void signal_handler(int sig) {
    keep_running = 0;
}

int main(void)
{
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    int ch = pipe_client_get_next_available_channel();
    if (ch < 0) {
        fprintf(stderr, "Failed to get pipe channel\n");
        return 1;
    }

    if (pipe_client_open(ch, PIPE_PATH, CLIENT_NAME,
                         EN_PIPE_CLIENT_AUTO_RECONNECT,
                         1024 * 1024) != 0) {
        fprintf(stderr, "Failed to open pipe: %s\n", PIPE_PATH);
        return 1;
    }

    int fd = pipe_client_get_fd(ch);
    if (fd < 0) {
        fprintf(stderr, "Failed to get pipe file descriptor\n");
        return 1;
    }

    fprintf(stderr, "mpa_reader started, reading from pipe: %s\n", PIPE_PATH);

    while (keep_running) {
        // Read camera frame metadata first
        camera_image_metadata_t metadata;
        ssize_t n = read(fd, &metadata, sizeof(metadata));
        if (n != sizeof(metadata)) {
            if (n < 0) {
                fprintf(stderr, "Error reading metadata\n");
                break;
            }
            fprintf(stderr, "Incomplete metadata, got %zd bytes\n", n);
            continue;
        }

        // Allocate buffer for RGB image data
        uint8_t *rgb_data = malloc(metadata.size_bytes);
        if (!rgb_data) {
            fprintf(stderr, "Failed to allocate %d bytes for RGB data\n", metadata.size_bytes);
            continue;
        }

        // Read RGB image data
        size_t bytes_read = 0;
        while (bytes_read < metadata.size_bytes && keep_running) {
            n = read(fd, rgb_data + bytes_read, metadata.size_bytes - bytes_read);
            if (n < 0) {
                fprintf(stderr, "Error reading RGB data\n");
                free(rgb_data);
                break;
            }
            if (n == 0) {
                fprintf(stderr, "End of stream reached\n");
                free(rgb_data);
                break;
            }
            bytes_read += n;
        }

        if (!keep_running) {
            free(rgb_data);
            break;
        }

        // Use exact model dimensions (no border removal)
        int mask_width = metadata.width;   // Use full width (320)
        int mask_height = metadata.height; // Use full height (320)
        int mask_size = mask_width * mask_height;

        // Allocate buffer for single channel mask
        uint8_t *mask_data = malloc(mask_size);
        if (!mask_data) {
            fprintf(stderr, "Failed to allocate %d bytes for mask data\n", mask_size);
            free(rgb_data);
            continue;
        }

        // Extract single channel (R channel) from RGB data
        for (int y = 0; y < mask_height; y++) {
            for (int x = 0; x < mask_width; x++) {
                // RGB data index (3 channels per pixel)
                int rgb_index = (y * mask_width + x) * 3;
                // Mask data index (1 channel per pixel)
                int mask_index = y * mask_width + x;
                
                // Take R channel (all channels are identical, so any channel works)
                mask_data[mask_index] = rgb_data[rgb_index];
            }
        }

        // Create output message structure
        UnetOutputMsg *output_msg = malloc(sizeof(UnetOutputMsg) + mask_size);
        if (!output_msg) {
            fprintf(stderr, "Failed to allocate output message\n");
            free(rgb_data);
            free(mask_data);
            continue;
        }

        // Fill output message
        output_msg->timestamp_ns = metadata.timestamp_ns;
        output_msg->width = mask_width;
        output_msg->height = mask_height;
        memcpy(output_msg->mask_data, mask_data, mask_size);

        // Output in the exact format Python expects:
        // 1. Header: timestamp (8 bytes) + width (4 bytes) + height (4 bytes)
        // 2. Mask data: mask_size bytes
        
        // Write header
        fwrite(&output_msg->timestamp_ns, sizeof(uint64_t), 1, stdout);
        fwrite(&output_msg->width, sizeof(int), 1, stdout);
        fwrite(&output_msg->height, sizeof(int), 1, stdout);
        
        // Write mask data
        fwrite(output_msg->mask_data, mask_size, 1, stdout);
        
        fflush(stdout);

        // Cleanup
        free(rgb_data);
        free(mask_data);
        free(output_msg);
        
        // Small delay to prevent excessive CPU usage
        usleep(10000); // 10ms
    }

    fprintf(stderr, "mpa_reader stopping\n");
    return 0;
}
