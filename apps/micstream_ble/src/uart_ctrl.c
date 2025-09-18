#include "uart_ctrl.h"

#include "ble_micstream.h"
#include "cmsis_os.h"
#include "config.h"
#include "hal_trace.h"
#include "hal_uart.h"

#include <stdbool.h>
#include <string.h>

#define UART_CMD_MAX 64

static osThreadId uart_thread_id = NULL;
static bool uart_ready = false;

static void uart_ctrl_thread(const void *arg);
osThreadDef(UART_CTRL, uart_ctrl_thread, osPriorityAboveNormal, 1, 1024, "mic_uart_ctrl");

static void uart_ctrl_thread(const void *arg) {
    (void)arg;
    char cmd[UART_CMD_MAX];
    size_t pos = 0;

    while (1) {
        uint8_t ch = hal_uart_blocked_getc(MICSTREAM_UART_ID);

        if (ch == '\r' || ch == '\n') {
            if (pos) {
                ble_micstream_on_ctrl((const uint8_t *)cmd, (uint16_t)pos);
                pos = 0;
            }
            continue;
        }

        if (pos + 1 >= sizeof(cmd)) {
            pos = 0;
            continue;
        }

        cmd[pos++] = (char)ch;
    }
}

void uart_ctrl_init(void) {
    struct HAL_UART_CFG_T cfg = {0};

    cfg.parity = HAL_UART_PARITY_NONE;
    cfg.stop = HAL_UART_STOP_BITS_1;
    cfg.data = HAL_UART_DATA_BITS_8;
    cfg.flow = HAL_UART_FLOW_CONTROL_NONE;
    cfg.tx_level = HAL_UART_FIFO_LEVEL_1_2;
    cfg.rx_level = HAL_UART_FIFO_LEVEL_1_2;
    cfg.baud = MICSTREAM_UART_BAUD;
    cfg.dma_rx = false;
    cfg.dma_tx = false;
    cfg.dma_rx_stop_on_err = false;

    if (hal_uart_open(MICSTREAM_UART_ID, &cfg)) {
        TRACE(0, "[micstream][uart] open failed");
        return;
    }

    uart_ready = true;

    if (!uart_thread_id) {
        uart_thread_id = osThreadCreate(osThread(UART_CTRL), NULL);
    }

    TRACE(1, "[micstream][uart] ready baud=%u", MICSTREAM_UART_BAUD);
}

void uart_ctrl_send(const uint8_t *data, uint32_t len) {
    if (!uart_ready || !data || !len) {
        return;
    }

    for (uint32_t i = 0; i < len; ++i) {
        hal_uart_blocked_putc(MICSTREAM_UART_ID, data[i]);
    }
}
