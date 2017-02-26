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

#ifndef MAIN_WS2812RMT_H_
#define MAIN_WS2812RMT_H_

#include <stdint.h>
#include <driver/gpio.h>
#include <driver/rmt.h>

/* A single color led */
typedef struct rgb_s {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} rgb_t;

inline bool rgb_equal(rgb_t left, rgb_t right) {
  return left.r == right.r && left.g == right.g && left.b == right.b;
}

/** Context used to reference a ws2812rmt channel */
typedef struct ws2812rmt_s* ws2812rmt_t;

/** Initializes a ws2812rmt channel with a given RMT channel and GPIO port */
ws2812rmt_t ws2812rmt_init(rmt_channel_t channel, gpio_num_t gpio_num);

/**
 * Sets the LEDs to individual colors.
 *
 * This function will only transmit the new colors once, and
 * will return once the new colors are transmitted.
 *
 * You can transmit a new set of colors once the function returns.
 */
void ws2812rmt_set_colors(ws2812rmt_t ws2812rmt, rgb_t* colors, int color_count);

/**
 * Sets all of the LEDs to the same color.
 *
 * This function will only transmit the new colors once, and
 * will return once the new colors are transmitted.
 *
 * You can transmit a new set of colors once the function returns.
 */
void ws2812rmt_set_one_color(ws2812rmt_t ws2812rmt, rgb_t color, int led_count);

/**
 * Sets the LEDs to a repeating pattern.
 *
 * This function will only transmit the new colors once, and
 * will return once the new colors are transmitted.
 *
 * You can transmit a new set of colors once the function returns.
 */
void ws2812rmt_set_pattern(ws2812rmt_t ws2812rmt, rgb_t* pattern, int color_count, int led_count);

#endif /* MAIN_WS2812RMT_H_ */
