#pragma once

#include <stdint.h>

#define MICSTREAM_MAGIC 0x4D50434D

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint8_t  version;
    uint8_t  node_id;
    uint8_t  codec;
    uint8_t  reserved;
    uint32_t fs_hz;
    uint16_t nsamples;
    uint16_t payload_bytes;
    uint64_t sample_counter;
    uint32_t seq;
} micstream_hdr_t;
#pragma pack(pop)
