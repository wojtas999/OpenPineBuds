#pragma once

#include <stdint.h>

#define UUID_SVC_MICSTREAM  "7d8f7f7a-1b36-4e2e-9a77-4b6b6c0b9a01"
#define UUID_CHR_AUDIO      "7d8f7f7a-1b36-4e2e-9a77-4b6b6c0b9a02"
#define UUID_CHR_CTRL       "7d8f7f7a-1b36-4e2e-9a77-4b6b6c0b9a03"

#ifdef __cplusplus
extern "C" {
#endif

void ble_micstream_init(void);
void ble_micstream_notify(const uint8_t *data, uint16_t len);
void ble_micstream_on_ctrl(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif
