#include "model_helper/model_helper.h"
#include "model_helper/posenet_model_helper.h"
#include "model_helper/yolov8_model_helper.h"
#include "model_helper/yolov5_model_helper.h"
#include "model_helper/fast_depth_model_helper.h"
#include "model_helper/generic_classification_model_helper.h"
#include "model_helper/generic_object_detection_model_helper.h"
#include "model_helper/deep_lab_model_helper.h"
#include "model_helper/gate_xyz_model_helper.h"
#include "model_helper/gate_yaw_model_helper.h"
#include "model_helper/gate_bin_model_helper.h"
#include "model_helper/zeroshot_model_helper.h"


ModelHelper *create_model_helper(ModelName model_name,
                                 ModelCategory model_category,
                                 DelegateOpt opt_,
                                 NormalizationType do_normalize)
{
    switch (model_name)
    {
    case POSENET:
    {
        if (model_category == POSE)
        {
            return new PoseNetModelHelper(model, labels_in_use, opt_, en_debug, en_timing, do_normalize);
        }
        else
        {
            fprintf(stderr, "Unsupported category for the given model\n");
        }
        break;
    }
    case YOLOV5:
    {
        if (model_category == OBJECT_DETECTION)
        {
            return new YoloV5ModelHelper(model, labels_in_use, opt_, en_debug, en_timing, do_normalize);
        }
        else
        {
            fprintf(stderr, "Unsupported category for the given model\n");
        }
        break;
    }
    case YOLOV8:
    {
        if (model_category == OBJECT_DETECTION)
        {

            return new YoloV8ModelHelper(model, labels_in_use, opt_, en_debug, en_timing, do_normalize);
        }
        else
        {
            fprintf(stderr, "Unsupported category for the given model\n");
        }
        break;
    }
    case MOBILE_NET:
    {
        if (model_category == OBJECT_DETECTION)
        {
            return new GenericObjectDetectionModelHelper(model, labels_in_use, opt_, en_debug, en_timing, do_normalize);
        }
        else if (model_category == CLASSIFICATION)
        {
            int tensor_offset = 1;
            return new GenericClassificationModelHelper(model, labels_in_use, opt_, en_debug, en_timing, do_normalize, tensor_offset);
        }
        else
        {
            fprintf(stderr, "Unsupported category for the given model\n");
        }
        break;
    }
    case FAST_DEPTH:
    {
        if (model_category == MONO_DEPTH)
        {
            return new FastDepthModelHelper(model, labels_in_use, opt_, en_debug, en_timing, do_normalize);
        }
        else
        {
            fprintf(stderr, "Unsupported category for the given model\n");
        }
        break;
    }
    case DEEPLAB:
    {
        if (model_category == SEGMENTATION)
        {
            return new DeepLabModelHelper(model, labels_in_use, opt_, en_debug, en_timing, do_normalize);
        }
        else
        {
            fprintf(stderr, "Unsupported category for the given model\n");
        }
    }

    case EFFICIENT_NET:
    {
        if (model_category == CLASSIFICATION)
        {
            int tensor_offset = 0;
            return new GenericClassificationModelHelper(model, labels_in_use, opt_, en_debug, en_timing, do_normalize, tensor_offset);
        }
        else
        {
            fprintf(stderr, "Unsupported category for the given model\n");
        }
    }

    case YOLOV11:
    {
        if (model_category == OBJECT_DETECTION)
        {
            // The usage for v8 and v11 is the same so the same api is used
            return new YoloV8ModelHelper(model, labels_in_use, opt_, en_debug, en_timing, do_normalize);
        }
        else
        {
            fprintf(stderr, "Unsupported category for the given model\n");
        }
    }
    case GATE_XYZ:
    {
        return new GateXyzModelHelper(
            model,
            labels_in_use,
            opt_,
            en_debug,
            en_timing,
            do_normalize
        );
    }
    case GATE_YAW:
    {
        return new GateYawModelHelper(
            model,
            labels_in_use,
            opt_,
            en_debug,
            en_timing,
            do_normalize
        );
    }
    case GATE_BIN:
    {
        return new GateBinModelHelper(
            model,
            labels_in_use,
            opt_,
            en_debug,
            en_timing,
            do_normalize
        );
    }
    // not sure about the utility of this enum
    // might remove later
    case ZEROSHOT:
    {
        if (model_category == OBJECT_DETECTION)
        {
            return new ZeroshotModelHelper(model, labels_in_use, opt_, en_debug, en_timing, do_normalize);
        }
        else
        {
            fprintf(stderr, "Unsupported category for the given model\n");
        }
        break;
    }
    case PLACEHOLDER:
    {
        if (model_category == OBJECT_DETECTION)
        {
            return new GenericObjectDetectionModelHelper(model, labels_in_use, opt_, en_debug, en_timing, do_normalize);
        }
        else
        {
            fprintf(stderr, "Unsupported category for the given model\n");
        }
    }
    }
    fprintf(stderr, "Unsupported model\n");
    return nullptr;
}
ModelHelper::ModelHelper(char *model_file, char *labels_file,
                         DelegateOpt delegate_choice, bool _en_debug,
                         bool _en_timing, NormalizationType _do_normalize)
{
    // Set the member variables
    en_debug = _en_debug;
    en_timing = _en_timing;
    do_normalize = _do_normalize;
    hardware_selection = delegate_choice;
    labels_location = labels_file;

    // Load the model
    model = tflite::FlatBufferModel::BuildFromFile(model_file);
    if (!model)
    {
        fprintf(stderr, "FATAL: Failed to mmap model %s\n", model_file);
        exit(-1);
    }

    if (en_debug)
        printf("Loaded model %s\n", model_file);

    // Resolve the reporter
    model->error_reporter();
    if (en_debug)
        printf("Resolved reporter\n");

    // Build the interpreter
    tflite::InterpreterBuilder(*model, resolver)(&interpreter);
    if (!interpreter)
    {
        fprintf(stderr, "Failed to construct interpreter\n");
        exit(-1);
    }

    // Set multi-threading
#ifdef BUILD_QRB5165
    interpreter->SetNumThreads(8);
#else
    interpreter->SetNumThreads(4);
#endif

    // Allow FP16 precision loss
    interpreter->SetAllowFp16PrecisionForFp32(true);

    // Setup optional hardware delegate
    setupDelegate(hardware_selection);

    // Allocate tensors
    if (interpreter->AllocateTensors() != kTfLiteOk)
    {
        fprintf(stderr, "Failed to allocate tensors!\n");
        exit(-1);
    }

    // Get model-specific parameters
    TfLiteIntArray *dims = interpreter->tensor(interpreter->inputs()[0])->dims;
    model_height = dims->data[1];
    model_width = dims->data[2];
    model_channels = dims->data[3];

    printf("Successfully built interpreter\n");
}

bool ModelHelper::preprocess(camera_image_metadata_t &meta,
                             char *frame, std::shared_ptr<cv::Mat> preprocessed_image,
                             std::shared_ptr<cv::Mat> output_image)
{
    start_time = rc_nanos_monotonic_time();
    num_frames_processed++;

    // initialize the resize map on first frame received only
    if (num_frames_processed == 1)
    {
        mcv_init_resize_map(meta.width, meta.height, model_width, model_height,
                            &map);
        input_height = meta.height;
        input_width = meta.width;

        if (meta.format == IMAGE_FORMAT_RAW8)
        {
            resize_output =
                (uint8_t *)malloc(model_height * model_width * sizeof(uint8_t));
        }
        else
        {
            resize_output = (uint8_t *)malloc(model_height * model_width *
                                              sizeof(uint8_t) * 3);
        }
        return false;
    }
    // if color input provided, make sure that is reflected in output image
    switch (meta.format)
    {
    case IMAGE_FORMAT_STEREO_NV12:
        meta.format = IMAGE_FORMAT_NV12;
    case IMAGE_FORMAT_NV12:
    {
        cv::Mat yuv(input_height + input_height / 2, input_width, CV_8UC1,
                    (uchar *)frame);
        cv::cvtColor(yuv, *output_image, CV_YUV2RGB_NV12);
        mcv_resize_8uc3_image(output_image->data, resize_output, &map);
        cv::Mat holder(model_height, model_width, CV_8UC3,
                       (uchar *)resize_output);

        *preprocessed_image = holder;
        meta.format = IMAGE_FORMAT_RGB;
        meta.size_bytes = (meta.height * meta.width * 3);
        meta.stride = (meta.width * 3);
    }
    break;
    case IMAGE_FORMAT_YUV422:
    {
        cv::Mat yuv(input_height, input_width, CV_8UC2, (uchar *)frame);
        cv::cvtColor(yuv, *output_image, CV_YUV2RGB_YUYV);

        // Resize to model input dimensions
        mcv_resize_8uc3_image(output_image->data, resize_output, &map);
        cv::Mat holder(model_height, model_width, CV_8UC3, (uchar *)resize_output);

        // Assign processed image and update meta data
        *preprocessed_image = holder;

        meta.format = IMAGE_FORMAT_RGB;
        meta.size_bytes = (meta.height * meta.width * 3);
        meta.stride = (meta.width * 3);
    }
    break;
    case IMAGE_FORMAT_STEREO_NV21:
        meta.format = IMAGE_FORMAT_NV21;
    case IMAGE_FORMAT_NV21:
    {
        cv::Mat yuv(input_height + input_height / 2, input_width, CV_8UC1,
                    (uchar *)frame);
        cv::cvtColor(yuv, *output_image, CV_YUV2RGB_NV21);
        mcv_resize_8uc3_image(output_image->data, resize_output, &map);
        cv::Mat holder(model_height, model_width, CV_8UC3,
                       (uchar *)resize_output);

        *preprocessed_image = holder;

        meta.format = IMAGE_FORMAT_RGB;
        meta.size_bytes = (meta.height * meta.width * 3);
        meta.stride = (meta.width * 3);
    }
    break;

    case IMAGE_FORMAT_STEREO_RAW8:
        meta.format = IMAGE_FORMAT_RAW8;
    case IMAGE_FORMAT_RAW8:
    {
        *output_image =
            cv::Mat(input_height, input_width, CV_8UC1, (uchar *)frame);

        // resize to model input dims
        mcv_resize_image(output_image->data, resize_output, &map);

        // stack resized input to make "3 channel" grayscale input
        cv::Mat holder(model_height, model_width, CV_8UC1,
                       (uchar *)resize_output);
        cv::Mat in[] = {holder, holder, holder};
        cv::merge(in, 3, *preprocessed_image);
    }
    break;

    default:
        fprintf(stderr,
                "Unexpected image format %d received! Exiting now.\n",
                meta.format);
        return false;
    }

    if (en_timing)
        total_preprocess_time +=
            ((rc_nanos_monotonic_time() - start_time) / 1000000.);

    return true;
}

void ModelHelper::setupDelegate(DelegateOpt delegate_choice)
{
    switch (delegate_choice)
    {
    case XNNPACK:
#ifdef BUILD_QRB5165
    {
        TfLiteXNNPackDelegateOptions xnnpack_options = TfLiteXNNPackDelegateOptionsDefault();
        xnnpack_options.num_threads = 8;
        xnnpack_delegate = TfLiteXNNPackDelegateCreate(&xnnpack_options);
        if (interpreter->ModifyGraphWithDelegate(xnnpack_delegate) != kTfLiteOk)
            fprintf(stderr, "Failed to apply XNNPACK delegate\n");
    }
#endif
    break;

    case GPU:
    {
        TfLiteGpuDelegateOptionsV2 gpu_opts = TfLiteGpuDelegateOptionsV2Default();
        gpu_opts.inference_preference = TFLITE_GPU_INFERENCE_PREFERENCE_SUSTAINED_SPEED;
        gpu_opts.inference_priority1 = TFLITE_GPU_INFERENCE_PRIORITY_MIN_LATENCY;
        gpu_delegate = TfLiteGpuDelegateV2Create(&gpu_opts);
        if (interpreter->ModifyGraphWithDelegate(gpu_delegate) != kTfLiteOk)
            fprintf(stderr, "Failed to apply GPU delegate\n");
    }
    break;

    case NNAPI:
#ifdef BUILD_QRB5165
    {
        const auto *nnapi_impl = NnApiImplementation();
        std::string temp = tflite::nnapi::GetStringDeviceNamesList(nnapi_impl);
        tflite::StatefulNnApiDelegate::Options nnapi_opts;
        nnapi_opts.execution_preference = tflite::StatefulNnApiDelegate::Options::ExecutionPreference::kSustainedSpeed;
        nnapi_opts.allow_fp16 = true;
        nnapi_opts.execution_priority = ANEURALNETWORKS_PRIORITY_HIGH;
        nnapi_opts.use_burst_computation = true;
        nnapi_opts.disallow_nnapi_cpu = false;
        nnapi_opts.max_number_delegated_partitions = -1;
        nnapi_opts.accelerator_name = "libunifiedhal-driver.so2";

        nnapi_delegate = new tflite::StatefulNnApiDelegate(nnapi_opts);
        if (interpreter->ModifyGraphWithDelegate(nnapi_delegate) != kTfLiteOk)
            fprintf(stderr, "Failed to apply NNAPI delegate\n");
    }
#endif
    break;
    }

}

bool ModelHelper::run_inference(cv::Mat &preprocessed_image,
                                double *last_inference_time)
{
    start_time = rc_nanos_monotonic_time();
    // Get input dimension from the input tensor metadata assuming one input
    // only
    int input = interpreter->inputs()[0];

    // manually fill tensor with image data, specific to input format
    switch (interpreter->tensor(input)->type)
    {
    case kTfLiteFloat32:
    {
        float *dst = TensorData<float>(interpreter->tensor(input), 0);
        const int row_elems = model_width * model_channels;
        for (int row = 0; row < model_height; row++)
        {
            const uchar *row_ptr = preprocessed_image.ptr(row);
            for (int i = 0; i < row_elems; i++)
            {
                if (do_normalize == HARD_DIVISION)
                    dst[i] = row_ptr[i] / NORMALIZATION_CONST;
                else if (do_normalize == PIXEL_MEAN)
                    dst[i] =
                        (row_ptr[i] - PIXEL_MEAN_GUESS) / PIXEL_MEAN_GUESS;
                else
                    dst[i] = (row_ptr[i]);
            }
            dst += row_elems;
        }
    }
    break;

    case kTfLiteInt8:
    {
        int8_t *dst = TensorData<int8_t>(interpreter->tensor(input), 0);
        const int row_elems = model_width * model_channels;
        for (int row = 0; row < model_height; row++)
        {
            const uchar *row_ptr = preprocessed_image.ptr(row);
            for (int i = 0; i < row_elems; i++)
            {
                dst[i] = row_ptr[i];
            }
            dst += row_elems;
        }
    }
    break;

    case kTfLiteUInt8:
    {
        uint8_t *dst = TensorData<uint8_t>(interpreter->tensor(input), 0);
        int row_elems = model_width * model_channels;
        for (int row = 0; row < model_height; row++)
        {
            uchar *row_ptr = preprocessed_image.ptr(row);
            for (int i = 0; i < row_elems; i++)
            {
                dst[i] = row_ptr[i];
            }
            dst += row_elems;
        }
    }
    break;

    default:
        fprintf(stderr, "FATAL: Unsupported model input type!");
        return false;
    }

    if (interpreter->Invoke() != kTfLiteOk)
    {
        fprintf(stderr, "FATAL: Failed to invoke tflite!\n");
        return false;
    }

    int64_t end_time = rc_nanos_monotonic_time();

    if (en_timing)
        total_inference_time += ((end_time - start_time) / 1000000.);
    if (last_inference_time != nullptr)
        *last_inference_time = ((double)(end_time - start_time) / 1000000.);

    return true;
}

void ModelHelper::print_summary_stats()
{
    fprintf(stderr, "\n------------------------------------------\n");
    fprintf(stderr, "TIMING STATS (on %d processed frames)\n",
            num_frames_processed);
    fprintf(stderr, "------------------------------------------\n");
    fprintf(stderr,
            "Preprocessing Time  -> Total: %6.2fms, Average: %6.2fms\n",
            (double)(total_preprocess_time),
            (double)((total_preprocess_time / (num_frames_processed))));
    fprintf(stderr,
            "Inference Time      -> Total: %6.2fms, Average: %6.2fms\n",
            (double)(total_inference_time),
            (double)((total_inference_time / (num_frames_processed))));
    fprintf(stderr,
            "Postprocessing Time -> Total: %6.2fms, Average: %6.2fms\n",
            (double)(total_postprocess_time),
            (double)((total_postprocess_time / (num_frames_processed))));
    fprintf(stderr, "------------------------------------------\n");
}