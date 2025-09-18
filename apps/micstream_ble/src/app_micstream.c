#include "app_micstream.h"

#include "audioflinger.h"
#include "ble_micstream.h"
#include "cmsis_os.h"
#include "config.h"
#include "hal_aud.h"
#include "hal_trace.h"
#include "ima_adpcm.h"
#include "micstream_proto.h"
#include "uart_ctrl.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define MICSTREAM_CODEC_IMA_CODE 1

static volatile uint64_t g_sc = 0;
static uint32_t g_seq = 0;
static uint32_t g_frames_sent = 0;
static uint32_t g_fs_hz = MICSTREAM_FS_HZ;
static bool g_streaming = false;
static bool g_stream_open = false;

static ima_state_t g_adpcm;
static uint8_t g_packet[64 + 1024];
static uint8_t g_capture_buf[MICSTREAM_NSAMPLES * sizeof(int16_t)];

static osTimerId g_log_timer = NULL;

static void micstream_log_timer_cb(void const *arg);
osTimerDef(MICSTREAM_LOG, micstream_log_timer_cb);

static enum AUD_SAMPRATE_T micstream_fs_to_enum(uint32_t fs) {
    switch (fs) {
    case 16000:
        return AUD_SAMPRATE_16000;
    case 24000:
        return AUD_SAMPRATE_24000;
    default:
        return AUD_SAMPRATE_16000;
    }
}

static void micstream_reset_counters(void) {
    g_sc = 0;
    g_seq = 0;
    g_frames_sent = 0;
    ima_init(&g_adpcm);
}

static void micstream_log_timer_cb(void const *arg) {
    (void)arg;
    if (g_streaming) {
        TRACE(2, "[micstream] frames=%u sc=%llu", g_frames_sent,
              (unsigned long long)g_sc);
        g_frames_sent = 0;
    }
}

static void micstream_timer_start(void) {
    if (!g_log_timer) {
        g_log_timer = osTimerCreate(osTimer(MICSTREAM_LOG), osTimerPeriodic, NULL);
    }
    if (g_log_timer) {
        osTimerStart(g_log_timer, 1000);
    }
}

static void micstream_timer_stop(void) {
    if (g_log_timer) {
        osTimerStop(g_log_timer);
    }
}

static uint32_t micstream_capture_cb(uint8_t *buf, uint32_t len) {
    if (!g_streaming) {
        return len;
    }

    uint16_t nsamples = len / sizeof(int16_t);
    if (!nsamples) {
        return len;
    }

    micstream_hdr_t *hdr = (micstream_hdr_t *)g_packet;
    hdr->magic = MICSTREAM_MAGIC;
    hdr->version = 1;
    hdr->node_id = MICSTREAM_NODE_ID;
    hdr->codec = MICSTREAM_CODEC_IMA_CODE;
    hdr->reserved = 0;
    hdr->fs_hz = g_fs_hz;
    hdr->nsamples = nsamples;
    hdr->payload_bytes = 0;
    hdr->sample_counter = g_sc;
    hdr->seq = g_seq++;

    int max_payload = (int)(sizeof(g_packet) - sizeof(*hdr));
    int paylen = ima_encode_block(&g_adpcm, (const int16_t *)buf, nsamples,
                                  g_packet + sizeof(*hdr), max_payload);
    if (paylen <= 0) {
        TRACE(0, "[micstream] ADPCM encode failed");
        return len;
    }

    hdr->payload_bytes = (uint16_t)paylen;

    ble_micstream_notify(g_packet, (uint16_t)(sizeof(*hdr) + paylen));
    uart_ctrl_send(g_packet, (uint32_t)(sizeof(*hdr) + paylen));

    g_sc += nsamples;
    g_frames_sent++;

    return len;
}

static void micstream_setup_cfg(struct AF_STREAM_CONFIG_T *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->bits = AUD_BITS_16;
    cfg->sample_rate = micstream_fs_to_enum(g_fs_hz);
    cfg->channel_num = AUD_CHANNEL_NUM_1;
    cfg->channel_map = AUD_CHANNEL_MAP_CH0;
    cfg->device = AUD_STREAM_USE_INT_CODEC;
    cfg->io_path = MICSTREAM_INPUT_PATH;
    cfg->handler = micstream_capture_cb;
    cfg->data_ptr = g_capture_buf;
    cfg->data_size = sizeof(g_capture_buf);
    cfg->vol = 0;
}

static void micstream_start_stream(void) {
    if (g_streaming) {
        return;
    }

    struct AF_STREAM_CONFIG_T cfg;
    micstream_setup_cfg(&cfg);

    micstream_reset_counters();

    if (af_stream_open(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE, &cfg)) {
        TRACE(0, "[micstream] af_stream_open failed");
        return;
    }
    g_stream_open = true;

    if (af_stream_start(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE)) {
        TRACE(0, "[micstream] af_stream_start failed");
        af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
        g_stream_open = false;
        return;
    }

    g_streaming = true;
    micstream_timer_start();
    TRACE(4, "[micstream] streaming Fs=%u ns=%u node=%u baud=%u", g_fs_hz,
          MICSTREAM_NSAMPLES, MICSTREAM_NODE_ID, MICSTREAM_UART_BAUD);
}

static void micstream_stop_stream(void) {
    if (!g_stream_open) {
        g_streaming = false;
        micstream_timer_stop();
        return;
    }

    g_streaming = false;
    micstream_timer_stop();
    af_stream_stop(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
    af_stream_close(AUD_STREAM_ID_0, AUD_STREAM_CAPTURE);
    g_stream_open = false;
    TRACE(0, "[micstream] stream stopped");
}

void micstream_on_start_cmd(void) {
    micstream_start_stream();
}

void micstream_on_stop_cmd(void) {
    micstream_stop_stream();
}

void micstream_on_set_fs_cmd(uint32_t fs_hz) {
    if (fs_hz != 16000 && fs_hz != 24000) {
        TRACE(1, "[micstream] ignore unsupported Fs=%u", fs_hz);
        return;
    }

    if (g_fs_hz == fs_hz) {
        return;
    }

    bool restart = g_streaming;
    micstream_stop_stream();
    g_fs_hz = fs_hz;
    TRACE(1, "[micstream] Fs changed to %u", fs_hz);
    if (restart) {
        micstream_start_stream();
    }
}

int app_init(void) {
    TRACE(3, "[micstream] init Fs=%u ns=%u node=%u", MICSTREAM_FS_HZ,
          MICSTREAM_NSAMPLES, MICSTREAM_NODE_ID);

    af_open();

    ble_micstream_init();
    uart_ctrl_init();

    return 0;
}

int app_deinit(int deinit_case) {
    (void)deinit_case;
    micstream_stop_stream();
    return 0;
}

int app_shutdown(void) {
    micstream_stop_stream();
    return 0;
}

int app_reset(void) {
    micstream_stop_stream();
    return 0;
}
