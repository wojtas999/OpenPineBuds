#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t predictor;
    int8_t step_index;
} ima_state_t;

void ima_init(ima_state_t *st);
int ima_encode_block(ima_state_t *st, const int16_t *pcm, int nsamples, uint8_t *out, int outcap);

#ifdef __cplusplus
}
#endif
