#include "ble_micstream.h"

#include "app_datapath_server.h"
#include "app_micstream.h"
#include "hal_trace.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bool ble_micstream_ctrl_handler(const uint8_t *data, uint16_t len);

void ble_micstream_init(void) {
    app_datapath_server_register_rx_callback(ble_micstream_ctrl_handler);
    TRACE(0, "[micstream] BLE MicStream service init");
}

void ble_micstream_notify(const uint8_t *data, uint16_t len) {
    if (!data || !len) {
        return;
    }

    if (app_datapath_server_env.connectionIndex == BLE_INVALID_CONNECTION_INDEX) {
        return;
    }

    if (!app_datapath_server_env.isNotificationEnabled) {
        return;
    }

    app_datapath_server_send_data_via_notification((uint8_t *)data, len);
}

void ble_micstream_on_ctrl(const uint8_t *data, uint16_t len) {
    if (!data || !len) {
        return;
    }

    char buf[64];
    uint16_t copy = len < sizeof(buf) - 1 ? len : (uint16_t)(sizeof(buf) - 1);
    memcpy(buf, data, copy);
    buf[copy] = '\0';

    // Normalize line endings and uppercase for comparison
    char *newline = strpbrk(buf, "\r\n");
    if (newline) {
        *newline = '\0';
    }

    if (!strcmp(buf, "START")) {
        micstream_on_start_cmd();
        return;
    }

    if (!strcmp(buf, "STOP")) {
        micstream_on_stop_cmd();
        return;
    }

    if (!strncasecmp(buf, "SET FS=", 7)) {
        unsigned long fs = strtoul(buf + 7, NULL, 10);
        if (fs == 16000 || fs == 24000) {
            micstream_on_set_fs_cmd((uint32_t)fs);
        } else {
            TRACE(1, "[micstream] unsupported Fs=%lu", fs);
        }
        return;
    }

    TRACE(1, "[micstream] unknown ctrl command: %s", buf);
}

static bool ble_micstream_ctrl_handler(const uint8_t *data, uint16_t len) {
    ble_micstream_on_ctrl(data, len);
    return true;
}
