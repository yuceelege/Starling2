#include <fstream>
#include <iostream>
#include <getopt.h>

#include "utils.h"
#include "config_file.h"


// Function to load labels from a file and ensure the list is padded to a multiple of 16
TfLiteStatus ReadLabelsFile(char *file_name, std::vector<std::string> *result,
                            size_t *found_label_count = nullptr)
{
    std::ifstream file(file_name);

    if (!file)
    {
        std::cerr << "Labels file " << file_name << " not found" << std::endl;
        return kTfLiteError;
    }

    result->clear();
    std::string line;
    while (std::getline(file, line))
    {
        result->push_back(line);
    }

    // Only update the count if found_label_count is provided
    // done to remove the unused variable warnings
    if (found_label_count != nullptr)
    {
        *found_label_count = result->size();
    }
    const int padding = 16;

    while (result->size() % padding)
    {
        result->emplace_back();
    }
    return kTfLiteOk;
}

uint64_t rc_nanos_monotonic_time()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000) + ts.tv_nsec;
}

bool _parse_opts(int argc, char *argv[], bool *en_debug, bool *en_timing)
{
    struct option long_options[] = {{"config", no_argument, 0, 'm'},
                                           {"debug", no_argument, 0, 'c'},
                                           {"timing", no_argument, 0, 't'},
                                           {"help", no_argument, 0, 'h'},
                                           {0, 0, 0}};
    while (1)
    {
        int option_index = 0;
        int c = getopt_long(argc, argv, "cdtph", long_options, &option_index);

        if (c == -1)
            break; // Detect the end of the options.

        switch (c)
        {
        case 0:
            // for long args without short equivalent that just set a flag
            // nothing left to do so just break.
            if (long_options[option_index].flag != 0)
                break;
            break;

        case 'c':
            config_file_read();
            exit(0);

        case 'd':
            printf("Enabling debug mode\n");
            *en_debug = true;
            break;

        case 't':
            printf("Enabling timing mode\n");
            *en_timing = true;
            break;

        case 'h':
            _print_usage();
            return true;

        default:
            // Print the usage if there is an incorrect command line option
            _print_usage();
            return true;
        }
    }
    return false;
}

void _print_usage()
{
    printf("\nCommand line arguments are as follows:\n\n");
    printf(
        "-c, --config    : load the config file only, for use by the config "
        "wizard\n");
    printf("-d, --debug     : enable verbose debug output (Default: Off)\n");
    printf(
        "-t, --timing    : enable timing output for model operations (Default: "
        "Off)\n");
    printf("-h              : Print this help message\n");
}


