#include "capture.h"
#include "config.h"

#include <stdlib.h>

int main(int argc, char **argv) {
    capture_config_t config;

    if (!config_parse(argc, argv, &config)) return EXIT_FAILURE;

    return capture_run(&config) ? EXIT_SUCCESS : EXIT_FAILURE;
}
