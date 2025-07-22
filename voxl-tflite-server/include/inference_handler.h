#ifndef INFERENCE_HANDLER_H
#define INFERENCE_HANDLER_H

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <modal_pipe.h>

#include "model_helper/model_helper.h"
#include "model_helper/model_info.h"

#define QUEUE_LIMIT       1

struct InferenceWorkerArgs
{
    ModelHelper *model_helper;
};

void *run_inference_pipeline(void *data);

// Setting up three threads
void preprocess_worker(ModelHelper *model_helper);
void inference_worker(ModelHelper *model_helper);
void postprocess_worker(ModelHelper *model_helper);

struct PipelineData
{
    camera_image_metadata_t metadata;
    std::shared_ptr<cv::Mat> preprocessed_image;
    std::shared_ptr<cv::Mat> output_image;
    double last_inference_time;
};


#endif