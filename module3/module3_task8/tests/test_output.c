#include "output.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char path[] = "/tmp/module3_task8_output_XXXXXX";
    const int temporary_fd = mkstemp(path);
    if (temporary_fd == -1) {
        fprintf(stderr, "Cannot create temporary output file\n");
        return 1;
    }
    close(temporary_fd);

    const capture_config_t config = {
        .interface_name = "eth0",
        .filter = CAPTURE_FILTER_CHAT,
        .duration_seconds = 10
    };
    capture_output_t output = {0};
    int status = output_open(&output, path);
    if (status) status = output_write_start(&output, &config);
    const udp_packet_t packet = {
        .source_mac = {0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB},
        .destination_mac = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55},
        .source_ip.s_addr = htonl(UINT32_C(0xC0A8010A)),
        .destination_ip.s_addr = htonl(UINT32_C(0xC0A801FF)),
        .source_port = 5000,
        .destination_port = 5000,
        .payload = (const uint8_t *)"data",
        .payload_size = 4
    };
    const capture_statistics_t statistics = {
        .frame_count = 42,
        .udp_count = 12,
        .matched_count = 4,
        .malformed_count = 3,
        .fragmented_count = 2,
        .unsupported_count = 25
    };
    if (status) status = output_write_packet(&output, &packet, 1, 0.125);
    if (status) status = output_write_summary(&output, &statistics, 1.25, 0);
    if (!output_close(&output)) status = 0;

    FILE *file = fopen(path, "r");
    char content[1024] = {0};
    if (file == NULL || fread(content, 1, sizeof(content) - 1U, file) == 0) status = 0;
    if (file != NULL) fclose(file);
    unlink(path);

    if (!status || strstr(content, "Interface: eth0") == NULL || strstr(content, "Filter: chat") == NULL ||
        strstr(content, "Packet #1 [0.125000 s]") == NULL || strstr(content, "Source MAC: 66:77:88:99:aa:bb") == NULL ||
        strstr(content, "Destination IP: 192.168.1.255") == NULL || strstr(content, "Ethernet frames received: 42") == NULL ||
        strstr(content, "UDP datagrams parsed: 12") == NULL || strstr(content, "Packets matched by filter: 4") == NULL ||
        strstr(content, "Malformed frames: 3") == NULL || strstr(content, "Stopped by: timer") == NULL) {
        fprintf(stderr, "Output report test failed\n");
        return 1;
    }

    puts("Output tests passed");
    return 0;
}
