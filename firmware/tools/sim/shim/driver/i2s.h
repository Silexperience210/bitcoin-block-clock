#pragma once
#include <Arduino.h>
typedef int i2s_port_t; typedef int i2s_mode_t; typedef int i2s_bits_per_sample_t;
typedef int i2s_channel_fmt_t; typedef int i2s_comm_format_t; typedef int i2s_channel_t;
#define I2S_NUM_0 0
#define I2S_MODE_MASTER 1
#define I2S_MODE_TX 4
#define I2S_BITS_PER_SAMPLE_16BIT 16
#define I2S_CHANNEL_FMT_RIGHT_LEFT 0
#define I2S_COMM_FORMAT_STAND_I2S 1
#define I2S_PIN_NO_CHANGE -1
#define I2S_CHANNEL_STEREO 2
#define I2S_CHANNEL_MONO 1
typedef struct { int mode, sample_rate, bits_per_sample, channel_format, communication_format, intr_alloc_flags, dma_buf_count, dma_buf_len; bool use_apll, tx_desc_auto_clear; int fixed_mclk; } i2s_config_t;
typedef struct { int bck_io_num, ws_io_num, data_out_num, data_in_num, mck_io_num; } i2s_pin_config_t;
inline int i2s_driver_install(int, const i2s_config_t *, int, void *) { return 0; }
inline int i2s_set_pin(int, const i2s_pin_config_t *) { return 0; }
inline int i2s_zero_dma_buffer(int) { return 0; }
inline int i2s_write(int, const void *, size_t n, size_t *w, uint32_t) { if (w) *w = n; return 0; }
inline int i2s_set_clk(int, uint32_t, int, int) { return 0; }
