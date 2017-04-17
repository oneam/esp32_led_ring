/*
 * Copyright 2017 Sam Leitch
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "ws2812rmt.h"
#include <stddef.h>
#include <limits.h>
#include <freertos/semphr.h>
#include "esp_log.h"

#define LOG_WS2812 "ws2812rmt"

/* Context for storing transmit data */
struct ws2812rmt_s {
  rmt_channel_t channel;
  int led_count; /* Count of LED color values */
  rmt_item32_t* tx_buffer; /* The RMT buffer for the channel */
  bool static_init;
};

struct ws2812rmt_s ws2812rmt_ctx[8];

/**
 *  80MHz clock is 12.5ns per tick
 *
 *  0.4us = 32 ticks
 *  0.45us = 36 ticks
 *  0.8us = 64 ticks
 *  0.85us = 68 ticks
 *  50us = 4000 ticks
 *
 *  For WS2812B:
 *  Zero = 0.4us high, 0.8us low
 *  One = 0.85us high, 0.45us low
 *  Reset = 50us low
 */

rmt_item32_t ws2812rmt_item_zero = { .duration0 = 32, .level0 = 1, .duration1 = 64, .level1 = 0 };
rmt_item32_t ws2812rmt_item_one = { .duration0 = 68, .level0 = 1, .duration1 = 36, .level1 = 0 };
rmt_item32_t ws2812rmt_item_reset = { .duration0 = 4000, .level0 = 0, .duration1 = 0, .level1 = 0 };

/** Initializes the RMT engine to transmit on a specified GPIO */
void ws2812rmt_init_rmt(rmt_channel_t channel, gpio_num_t gpio_num) {
  rmt_config_t rmt_tx;
  rmt_tx.channel = channel;
  rmt_tx.gpio_num = gpio_num;
  rmt_tx.mem_block_num = 1; /* Number of memory blocks. Not memory block number. */
  rmt_tx.clk_div = 1;
  rmt_tx.tx_config.loop_en = false;
  rmt_tx.tx_config.carrier_en = false;
  rmt_tx.tx_config.carrier_freq_hz = 0;
  rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  rmt_tx.tx_config.idle_output_en = true;
  rmt_tx.rmt_mode = RMT_MODE_TX;
  rmt_config(&rmt_tx);

  ESP_ERROR_CHECK(rmt_driver_install(channel, 0, 0));
}


ws2812rmt_t ws2812rmt_init(rmt_channel_t channel, gpio_num_t gpio_num, int led_count) {
  ESP_LOGI(LOG_WS2812, "Initializing ws2812rmt context with channel %d and gpio_num %d", channel, gpio_num);
  if(led_count <= 0) {
    ESP_LOGE(LOG_WS2812, "led_count %d is invalid", led_count);
    return NULL;
  }

  ws2812rmt_t ctx = ws2812rmt_ctx + channel;
  ctx->channel = channel;
  ctx->led_count = led_count;
  ctx->static_init = false;
  size_t buffer_size = (led_count * 24 + 1) * sizeof(rmt_item32_t);
  ctx->tx_buffer = malloc(buffer_size);
  if(!ctx->tx_buffer) {
    ESP_LOGE(LOG_WS2812, "Failed to allocate %d byte buffer", buffer_size);
    return NULL;
  }

  ws2812rmt_init_rmt(channel, gpio_num);
  return ctx;
}


ws2812rmt_t ws2812rmt_init_static(rmt_channel_t channel, gpio_num_t gpio_num, int led_count, rmt_item32_t *tx_buffer) {
  ESP_LOGI(LOG_WS2812, "Initializing ws2812rmt context with channel %d and gpio_num %d", channel, gpio_num);
  if(led_count <= 0) {
    ESP_LOGE(LOG_WS2812, "led_count %d is invalid", led_count);
    return NULL;
  }

  if(!tx_buffer) {
    ESP_LOGE(LOG_WS2812, "tx_buffer is invalid");
    return NULL;
  }

  ws2812rmt_t ctx = ws2812rmt_ctx + channel;
  ctx->channel = channel;
  ctx->led_count = led_count;
  ctx->static_init = false;
  ctx->tx_buffer = tx_buffer;

  ws2812rmt_init_rmt(channel, gpio_num);
  return ctx;
}


/** Adds 8 RMT items to the tx buffer representing a single byte */
void ws2812rmt_set_byte(ws2812rmt_t ctx, uint8_t value, int item_index) {
  // Iterate through each bit starting at MSB (Offset by start value)
  uint8_t mask = 0x80;

  while (mask > 0) {
    rmt_item32_t item_for_masked_bit = (value & mask) > 0 ? ws2812rmt_item_one : ws2812rmt_item_zero;
    ctx->tx_buffer[item_index] = item_for_masked_bit;
    mask >>= 1;
    ++item_index;
  }
}


/** Adds 24 RMT items to the tx buffer based on the given color value */
void ws2812rmt_set_color(ws2812rmt_t ctx, rgb_t color, int led_index) {
  int item_index = led_index * 24;

  ws2812rmt_set_byte(ctx, color.g, item_index);
  ws2812rmt_set_byte(ctx, color.r, item_index + 8);
  ws2812rmt_set_byte(ctx, color.b, item_index + 16);
}


/** Adds 1 RMT item indicating that the transmission should end. */
void ws2812rmt_set_reset(ws2812rmt_t ctx, int led_index) {
  int item_index = led_index * 24;

  ctx->tx_buffer[item_index] = ws2812rmt_item_reset;
}


void ws2812rmt_set_colors(ws2812rmt_t ctx, rgb_t* colors, int color_count, bool repeat) {
  ESP_LOGD(LOG_WS2812, "set_colors color_count = %d, repeat = %d", color_count, repeat);
  if(!ctx) {
    ESP_LOGE(LOG_WS2812, "ctx is invalid");
    return;
  }

  if(color_count <= 0 || color_count > ctx->led_count) {
    ESP_LOGE(LOG_WS2812, "color_count %d is invalid", color_count);
    return;
  }

  int num_values = ctx->led_count;
  if(color_count < num_values && !repeat) num_values = color_count;

  for(int i=0; i < num_values; ++i) {
    int color_index = i;
    if(color_index >= color_count) color_index %= color_count;

    ws2812rmt_set_color(ctx, colors[color_index], i);
  }

  ws2812rmt_set_reset(ctx, num_values);

  int num_items = num_values * 24 + 1;
  rmt_write_items(ctx->channel, ctx->tx_buffer, num_items, true);
}


void ws2812rmt_uninit(ws2812rmt_t *ctx) {
  if(!ctx) return;
  if(!(*ctx)->static_init) free((*ctx)->tx_buffer);
  ctx = NULL;
}
