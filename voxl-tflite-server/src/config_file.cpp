#include "config_file.h"

char model[CHAR_BUF_SIZE];
char input_pipe[CHAR_BUF_SIZE];
char control_input_pipe[CHAR_BUF_SIZE];
char delegate[CHAR_BUF_SIZE];
int skip_n_frames;
bool allow_multiple;
char output_pipe_prefix[CHAR_BUF_SIZE];
char labels_in_use[CHAR_BUF_SIZE];

void config_file_print(void)
{
    printf("=================================================================\n");
    printf("skip_n_frames:                    %d\n", skip_n_frames);
    printf("=================================================================\n");
    printf("model:                            %s\n", model);
    printf("=================================================================\n");
    printf("input_pipe:                       %s\n", input_pipe);
    printf("=================================================================\n");
    printf("control_input_pipe:               %s\n", control_input_pipe);
    printf("=================================================================\n");
    printf("delegate:                         %s\n", delegate);
    printf("=================================================================\n");
#ifdef BUILD_QRB5165
    printf("allow_multiple:                   %s\n", allow_multiple ? "true" : "false");
    printf("=================================================================\n");
    printf("output_pipe_prefix:               %s\n", output_pipe_prefix);
    printf("=================================================================\n");
#endif
    return;
}

int config_file_read(void)
{
    int ret = json_make_empty_file_with_header_if_missing(CONFIG_FILE, CONFIG_FILE_HEADER);
    if (ret < 0)
        return -1;
    else if (ret > 0)
        fprintf(stderr, "Creating new config file: %s\n", CONFIG_FILE);

    cJSON *parent = json_read_file(CONFIG_FILE);
    if (parent == NULL)
        return -1;

    // actually parse values
    json_fetch_int_with_default(parent, "skip_n_frames", &skip_n_frames, 0);
    json_fetch_string_with_default(parent, "model", model, CHAR_BUF_SIZE, "/usr/bin/dnn/ssdlite_mobilenet_v2_coco.tflite");
    json_fetch_string_with_default(parent, "input_pipe", input_pipe, CHAR_BUF_SIZE, "/run/mpa/hires_small_color/");
    json_fetch_string_with_default(parent, "control_input_pipe", control_input_pipe, CHAR_BUF_SIZE, "/run/mpa/control_out");
    json_fetch_string_with_default(parent, "delegate", delegate, CHAR_BUF_SIZE, "gpu");

    int requires_labels = 0;
    json_fetch_bool_with_default(parent, "requires_labels", &requires_labels, 1);

    if (requires_labels)
    {
        json_fetch_string_with_default(parent, "labels", labels_in_use, CHAR_BUF_SIZE, "/usr/bin/dnn/coco_labels.txt");
    }
    else
    {
        labels_in_use[0] = '\0';
    }

#ifdef BUILD_QRB5165
    json_fetch_bool_with_default(parent, "allow_multiple", (int *)&allow_multiple, 0);
    json_fetch_string_with_default(parent, "output_pipe_prefix", output_pipe_prefix, CHAR_BUF_SIZE, "mobilenet");
#endif

    if (json_get_parse_error_flag())
    {
        fprintf(stderr, "failed to parse config file %s\n", CONFIG_FILE);
        cJSON_Delete(parent);
        return -1;
    }

    // write modified data to disk if neccessary
    if (json_get_modified_flag())
    {
        printf("The config file was modified during parsing, saving the changes to disk\n");
        json_write_to_file_with_header(CONFIG_FILE, parent, CONFIG_FILE_HEADER);
    }
    cJSON_Delete(parent);
    return 0;
}