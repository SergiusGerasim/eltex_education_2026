#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int test_required_arguments(void) {
    char *argv[] = {"module3_task8", "eth0", "chat", "capture.txt"};
    capture_config_t config;

    if (!config_parse(4, argv, &config)) return 0;
    if (strcmp(config.interface_name, "eth0") != 0) return 0;
    if (config.filter != CAPTURE_FILTER_CHAT) return 0;
    if (strcmp(config.output_path, "captures/capture.txt") != 0) return 0;
    return config.duration_seconds == 0;
}

static int test_duration_and_dns_filter(void) {
    char *argv[] = {"module3_task8", "wlan0", "dns", "dns.txt", "30"};
    capture_config_t config;

    if (!config_parse(5, argv, &config)) return 0;
    if (config.filter != CAPTURE_FILTER_DNS) return 0;
    if (config.duration_seconds != UINT32_C(30)) return 0;
    return strcmp(config_filter_name(config.filter), "dns") == 0;
}

static int test_invalid_arguments(void) {
    capture_config_t config;
    char *too_few[] = {"module3_task8", "eth0", "chat"};
    char *bad_filter[] = {"module3_task8", "eth0", "http", "capture.txt"};
    char *bad_duration[] = {"module3_task8", "eth0", "chat", "capture.txt", "-1"};
    char *large_duration[] = {"module3_task8", "eth0", "chat", "capture.txt", "4294967296"};
    char *path_instead_of_name[] = {"module3_task8", "eth0", "chat", "other/capture.txt"};

    if (config_parse(3, too_few, &config)) return 0;
    if (config_parse(4, bad_filter, &config)) return 0;
    if (config_parse(5, bad_duration, &config)) return 0;
    if (config_parse(5, large_duration, &config)) return 0;
    if (config_parse(4, path_instead_of_name, &config)) return 0;
    return !config_parse(4, bad_filter, NULL);
}

int main(void) {
    if (!test_required_arguments()) {
        fprintf(stderr, "Required argument test failed\n");
        return 1;
    }

    if (!test_duration_and_dns_filter()) {
        fprintf(stderr, "Duration and DNS filter test failed\n");
        return 1;
    }

    if (!test_invalid_arguments()) {
        fprintf(stderr, "Invalid argument test failed\n");
        return 1;
    }

    puts("Config tests passed");
    return 0;
}
