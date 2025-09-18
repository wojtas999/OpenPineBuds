/*
 * Minimal IMA ADPCM encoder.
 *
 * Copyright (c) 2024 MicStream Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "ima_adpcm.h"

#include <stddef.h>

static const int16_t step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41,
    45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209,
    230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876,
    963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,
    10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086,
    29794, 32767,
};

static const int8_t index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
};

void ima_init(ima_state_t *st) {
    if (!st) {
        return;
    }

    st->predictor = 0;
    st->step_index = 0;
}

int ima_encode_block(ima_state_t *st, const int16_t *pcm, int nsamples, uint8_t *out, int outcap) {
    if (!st || !pcm || !out || nsamples <= 0 || outcap <= 0) {
        return 0;
    }

    int produced = 0;

    for (int i = 0; i < nsamples; i += 2) {
        if (produced >= outcap) {
            break;
        }

        uint8_t byte = 0;

        for (int n = 0; n < 2; ++n) {
            if (i + n >= nsamples) {
                byte |= 0x0F;
                continue;
            }

            int diff = pcm[i + n] - st->predictor;
            int sign = 0;
            if (diff < 0) {
                sign = 8;
                diff = -diff;
            }

            int step = step_table[st->step_index];
            int delta = 0;
            int vpdiff = step >> 3;

            if (diff >= step) {
                delta |= 4;
                diff -= step;
                vpdiff += step;
            }
            step >>= 1;
            if (diff >= step) {
                delta |= 2;
                diff -= step;
                vpdiff += step;
            }
            step >>= 1;
            if (diff >= step) {
                delta |= 1;
                vpdiff += step;
            }

            if (sign) {
                vpdiff = -vpdiff;
            }

            st->predictor += vpdiff;
            if (st->predictor > 32767) {
                st->predictor = 32767;
            } else if (st->predictor < -32768) {
                st->predictor = -32768;
            }

            st->step_index += index_table[delta | sign];
            if (st->step_index < 0) {
                st->step_index = 0;
            } else if (st->step_index > 88) {
                st->step_index = 88;
            }

            uint8_t code = (uint8_t)(delta | sign);
            if (n == 0) {
                byte = (uint8_t)(code << 4);
            } else {
                byte |= code;
            }
        }

        out[produced++] = byte;
    }

    return produced;
}
