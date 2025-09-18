#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_ctrl_init(void);
void uart_ctrl_send(const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif
