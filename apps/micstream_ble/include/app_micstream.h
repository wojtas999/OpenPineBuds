#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void micstream_on_start_cmd(void);
void micstream_on_stop_cmd(void);
void micstream_on_set_fs_cmd(uint32_t fs_hz);

#ifdef __cplusplus
}
#endif
