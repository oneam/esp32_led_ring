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

#ifndef MAIN_LED_RING_H_
#define MAIN_LED_RING_H_

#include "ws2812rmt.h"

typedef struct led_ring_s* led_ring_t;

led_ring_t led_ring_init(rmt_channel_t channel, gpio_num_t gpio, int led_count, rgb_t* led_color_buffer);

rgb_t* led_ring_get_color_buffer(led_ring_t ctx);

/** A spinner loop spins the pattern around the loop */
void led_ring_start_spinner_loop(led_ring_t ctx);

/** A strobing loop cycles through each color and sets all leds that color */
void led_ring_start_strobing_loop(led_ring_t ctx);

/** Stop the loop */
void led_ring_stop_loop(led_ring_t ctx);

void led_ring_set_one_color(led_ring_t ctx, rgb_t color);
void led_ring_set_colors(led_ring_t ctx, rgb_t* colors);
void led_ring_set_pattern(led_ring_t ctx, rgb_t* pattern, int color_count);
void led_ring_set_rainbow(led_ring_t ctx, int max_brightness);

#endif /* MAIN_LED_RING_H_ */
