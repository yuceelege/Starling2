#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>
#include "tensorflow/lite/c/c_api_internal.h" 

// Function to read labels file and pad the result to be a multiple of 16
TfLiteStatus ReadLabelsFile(char *file_name, std::vector<std::string> *result,
                            size_t *found_label_count);

// timing helper
uint64_t rc_nanos_monotonic_time();

bool _parse_opts(int argc, char *argv[], bool *en_debug, bool *en_timing);

void _print_usage();

#endif // UTILS_H