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
#include <stdbool.h>
#include <limits.h>
#include <freertos/semphr.h>
#include "esp_log.h"

/* Context for storing transmit data */
struct ws2812rmt_s {
	rmt_channel_t channel;
	rgb_t* colors; /* Requested LED color values */
  int color_count; /* Count of LED color values */
  int led_count; /* Count of LED color values */
	int item32_count; /* Total number of rmt_item32_t values that represent the colors */
	int item32_offset; /* Offset of rmt_item32_t values that have been written */
	volatile rmt_item32_t* tx_buffer; /* The RMT buffer for the channel */
	int tx_buffer_offset; /* Offset within the RMT buffer to start writing */
	int tx_buffer_limit; /* Value of tx_buffer_offset where writing should stop */
	SemaphoreHandle_t tx_semaphore; /* Semaphore for waiting for tx to complete */
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
rmt_item32_t ws2812rmt_item_end = { .duration0 = 0, .level0 = 0, .duration1 = 0, .level1 = 0 };

/* Values used in buffered write */
#define RMT_BUFFER_END RMT_MEM_ITEM_NUM
#define RMT_BUFFER_MIDDLE (RMT_MEM_ITEM_NUM / 2)
#define RMT_HALF_BUFFER (RMT_MEM_ITEM_NUM / 2)

void ws2812rmt_isr_handler(void* arg);

volatile rmt_item32_t* ws2812rmt_get_buffer(rmt_channel_t channel) {
	return RMTMEM.chan[channel].data32;
}

/**
 * Initializes the RMT engine to transmit on a specified GPIO
 */
ws2812rmt_t ws2812rmt_init(rmt_channel_t channel, gpio_num_t gpio_num) {
	ESP_LOGI("ws2812rmt", "Initializing ws2812rmt context with channel %d and gpio_num %d", channel, gpio_num);
	rmt_config_t rmt_tx;
	rmt_tx.channel = channel;
	rmt_tx.gpio_num = gpio_num;
	rmt_tx.mem_block_num = 1;	/* Number of memory blocks. Not memory block number. */
	rmt_tx.clk_div = 1;
	rmt_tx.tx_config.loop_en = false;
	rmt_tx.tx_config.carrier_en = false;
	rmt_tx.tx_config.carrier_freq_hz = 0;
	rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
	rmt_tx.tx_config.idle_output_en = true;
	rmt_tx.rmt_mode = RMT_MODE_TX;
	rmt_config(&rmt_tx);

	ws2812rmt_t ctx = ws2812rmt_ctx + channel;
	ctx->channel = channel;
	ctx->tx_buffer = ws2812rmt_get_buffer(channel);
	ctx->tx_semaphore = xSemaphoreCreateBinary();

	rmt_isr_register(ws2812rmt_isr_handler, ctx, 0, NULL);
	rmt_set_tx_intr_en(ctx->channel, true);

	return ctx;
}

/**
 * Adds 1 RMT item to the tx buffer if there is room
 *
 * @return True if the item could be added.
 */
bool ws2812rmt_add_item(ws2812rmt_t ctx, rmt_item32_t item) {
	if(ctx->tx_buffer_offset >= ctx->tx_buffer_limit) return false;

	ctx->tx_buffer[ctx->tx_buffer_offset] = item;
	++ctx->tx_buffer_offset;
	++ctx->item32_offset;
	return true;
}

/**
 * Adds 8 RMT items based on the given color component value
 *
 * @return True if all items for the color component are written to tx_buffer
 */
bool ws2812rmt_add_color_component(ws2812rmt_t ctx, uint8_t color_value, size_t component_offset) {
	// Iterate through each bit starting at MSB (Offset by start value)
	uint8_t mask = 0x80 >> component_offset;

	while (mask > 0) {
		rmt_item32_t item_for_masked_bit = (color_value & mask) > 0 ? ws2812rmt_item_one : ws2812rmt_item_zero;
		if(!ws2812rmt_add_item(ctx, item_for_masked_bit)) return false;
		mask >>= 1;
	}

	return true;
}

/**
 * Adds 24 RMT items based on the given color value
 *
 * @return True if all items for this color are complete
 */
bool ws2812rmt_add_color(ws2812rmt_t ctx, int color_offset) {
	rgb_t color = ctx->colors[color_offset];
	int component_offset = ctx->item32_offset % 24;

	if(component_offset < 8) {
		if(!ws2812rmt_add_color_component(ctx, color.g, component_offset)) return false;
		if(!ws2812rmt_add_color_component(ctx, color.r, 0)) return false;
		if(!ws2812rmt_add_color_component(ctx, color.b, 0)) return false;
	} else if(component_offset < 16) {
		if(!ws2812rmt_add_color_component(ctx, color.r, component_offset - 8)) return false;
		if(!ws2812rmt_add_color_component(ctx, color.b, 0)) return false;
	} else {
		if(!ws2812rmt_add_color_component(ctx, color.b, component_offset - 16)) return false;
	}

	return true;
}

/**
 * Adds all available RMT items to the tx_buffer
 *
 * @return True if all data was written to tx_buffer
 */
bool ws2812rmt_add_colors(ws2812rmt_t ctx) {
  int led_offset = ctx->item32_offset / 24;
  int color_offset = led_offset % ctx->color_count;

	while(led_offset < ctx->led_count) {
		if(!ws2812rmt_add_color(ctx, color_offset)) return false;
    ++led_offset;
    ++color_offset;
    if(color_offset >= ctx->color_count) color_offset = 0;
	}

	if(!ws2812rmt_add_item(ctx, ws2812rmt_item_reset)) return false;

	return true;
}

/* TX interrupt handler */
void ws2812rmt_isr_handler(void* arg) {
	ws2812rmt_t ctx = (ws2812rmt_t)arg;

	// If writing is complete let the update function complete
	if(RMT.int_st.ch0_tx_end) {
		ctx->colors = NULL;
		ctx->color_count = 0;

	    RMT.int_clr.ch0_tx_end = 1;

	    BaseType_t xHigherPriorityTaskWoken;
		xSemaphoreGiveFromISR(ctx->tx_semaphore, &xHigherPriorityTaskWoken);
        if(xHigherPriorityTaskWoken == pdTRUE) portYIELD_FROM_ISR();
	}

	if (RMT.int_st.ch0_tx_thr_event) {
		if(ctx->tx_buffer_limit == RMT_BUFFER_END) {
			// If we just wrote the last half, set ctx write the first half
			ctx->tx_buffer_offset = 0;
			ctx->tx_buffer_limit = RMT_BUFFER_MIDDLE;
		} else {
			// If we just wrote the first half, set ctx write the last half
			ctx->tx_buffer_offset = RMT_BUFFER_MIDDLE;
			ctx->tx_buffer_limit = RMT_BUFFER_END;
		}

		// Write more color data to the RMT buffer
		ws2812rmt_add_colors(ctx);

	    RMT.int_clr.ch0_tx_thr_event = 1;
	}
}

void ws2812rmt_calculate_item32_count(ws2812rmt_t ctx) {
	ctx->item32_count = ctx->led_count * 24 + 1; // 24 bits per color + 1 reset item
}

void ws2812rmt_update(ws2812rmt_t ctx) {
  bool all_items_fit = ws2812rmt_add_colors(ctx);

  if(all_items_fit) {
    // If everything fits in the buffer, don't set a threshold interrupt
    rmt_set_tx_thr_intr_en(ctx->channel, false, 0);
  } else {
    // Trigger an interrupt every time half of the buffer is transmitted
    rmt_set_tx_thr_intr_en(ctx->channel, true, RMT_BUFFER_MIDDLE);
  }

  rmt_tx_start(ctx->channel, true);
  xSemaphoreTake(ctx->tx_semaphore, portMAX_DELAY);
  ESP_LOGD("ws2812rmt", "update complete");
}

void ws2812rmt_set_colors(ws2812rmt_t ctx, rgb_t* colors, int color_count) {
  ESP_LOGD("ws2812rmt", "updating with %d colors", color_count);
  ctx->colors = colors;
  ctx->color_count = color_count;
  ctx->led_count = color_count;
  ctx->item32_offset = 0;
  ctx->tx_buffer_offset = 0;
  ctx->tx_buffer_limit = RMT_BUFFER_END;

  ws2812rmt_calculate_item32_count(ctx);
  ws2812rmt_update(ctx);
}

void ws2812rmt_set_one_color(ws2812rmt_t ctx, rgb_t color, int led_count) {
  ESP_LOGD("ws2812rmt", "updating all %d LEDs with color 0x%02x%02x%02x", led_count, color.r, color.g, color.b);
  ctx->colors = &color;
  ctx->color_count = 1;
  ctx->led_count = led_count;
  ctx->item32_offset = 0;
  ctx->tx_buffer_offset = 0;
  ctx->tx_buffer_limit = RMT_BUFFER_END;

  ws2812rmt_calculate_item32_count(ctx);
  ws2812rmt_update(ctx);
}

void ws2812rmt_set_pattern(ws2812rmt_t ctx, rgb_t* pattern, int color_count, int led_count) {
  ESP_LOGD("ws2812rmt", "updating with %d color pattern on %d LEDs", color_count, led_count);
  ctx->colors = pattern;
  ctx->color_count = color_count;
  ctx->led_count = led_count;
  ctx->item32_offset = 0;
  ctx->tx_buffer_offset = 0;
  ctx->tx_buffer_limit = RMT_BUFFER_END;

  ws2812rmt_calculate_item32_count(ctx);
  ws2812rmt_update(ctx);
}
