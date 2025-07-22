#ifndef TENSOR_DATA_H
#define TENSOR_DATA_H

#include "tensorflow/lite/examples/label_image/bitmap_helpers.h"

template <typename T>
T *TensorData(TfLiteTensor *tensor, int batch_index);

// Gets the float tensor data pointer
template <>
inline float *TensorData(TfLiteTensor *tensor, int batch_index)
{
    int nelems = 1;

    for (int i = 1; i < tensor->dims->size; i++)
    {
        nelems *= tensor->dims->data[i];
    }

    switch (tensor->type)
    {
    case kTfLiteFloat32:
        return tensor->data.f + nelems * batch_index;
    default:
        fprintf(stderr, "Error in %s: should not reach here\n",
                __FUNCTION__);
    }

    return nullptr;
}

// Gets the int32_t tensor data pointer
template <>
inline int32_t *TensorData(TfLiteTensor *tensor, int batch_index)
{
    int nelems = 1;

    for (int i = 1; i < tensor->dims->size; i++)
    {
        nelems *= tensor->dims->data[i];
    }

    switch (tensor->type)
    {
    case kTfLiteInt32:
        return tensor->data.i32 + nelems * batch_index;
    default:
        fprintf(stderr, "Error in %s: should not reach here\n",
                __FUNCTION__);
    }

    return nullptr;
}

// Gets the int64_t tensor data pointer
template <>
inline int64_t *TensorData(TfLiteTensor *tensor, int batch_index)
{
    int nelems = 1;

    for (int i = 1; i < tensor->dims->size; i++)
    {
        nelems *= tensor->dims->data[i];
    }

    switch (tensor->type)
    {
    case kTfLiteInt64:
        return tensor->data.i64 + nelems * batch_index;
    default:
        fprintf(stderr, "Error in %s: should not reach here\n",
                __FUNCTION__);
    }

    return nullptr;
}

// Gets the int8_t tensor data pointer
template <>
inline int8_t *TensorData(TfLiteTensor *tensor, int batch_index)
{
    int nelems = 1;

    for (int i = 1; i < tensor->dims->size; i++)
    {
        nelems *= tensor->dims->data[i];
    }

    switch (tensor->type)
    {
    case kTfLiteInt8:
        return tensor->data.int8 + nelems * batch_index;
    default:
        fprintf(stderr, "Error in %s: should not reach here\n",
                __FUNCTION__);
    }

    return nullptr;
}

// Gets the uint8_t tensor data pointer
template <>
inline uint8_t *TensorData(TfLiteTensor *tensor, int batch_index)
{
    int nelems = 1;

    for (int i = 1; i < tensor->dims->size; i++)
    {
        nelems *= tensor->dims->data[i];
    }

    switch (tensor->type)
    {
    case kTfLiteUInt8:
        return tensor->data.uint8 + nelems * batch_index;
    default:
        fprintf(stderr, "Error in %s: should not reach here\n",
                __FUNCTION__);
    }

    return nullptr;
}

#endif