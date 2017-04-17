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

#include "led_ring.h"

#include <freertos/freeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#define MAX_LED_RINGS 8
#define LOG_LEDRING "led_ring"

struct led_ring_s {
  ws2812rmt_t ws2812;
  int led_count;
  rgb_t* led_color_buffer;
  TaskHandle_t loop_task;
  SemaphoreHandle_t loop_semaphore;
  volatile bool strobing;
  volatile bool spinning;
  volatile bool animating;
};

struct led_ring_s led_rings[MAX_LED_RINGS];

rgb_t* led_ring_get_color_buffer(led_ring_t ctx) {
  return ctx->led_color_buffer;
}

#define RAINBOW_SECTION_RED_TO_YELLOW   0
#define RAINBOW_SECTION_YELLOW_TO_GREEN 1
#define RAINBOW_SECTION_GREEN_TO_CYAN   2
#define RAINBOW_SECTION_CYAN_TO_BLUE    3
#define RAINBOW_SECTION_BLUE_TO_MAGENTA 4
#define RAINBOW_SECTION_MAGENTA_TO_RED  5

/**
 * Returns a color for a rainbow of colors of any size.
 *
 * The rainbow has 6 sections based on 6 color transitions.
 * Within each section:
 * * 2 of the RGB values are fixed at max_brightness or 0
 * * 1 of the RGB values is in transition from max_brightness to 0 or 0 to max_brightness
 */
static rgb_t led_ring_calculate_rainbow_color(int index, int count, int max_brightness) {
  rgb_t color = { 0, 0, 0 };

  /* All of this math is to compensate for rainbow that are not even multiples of 6 in length */
  int section_num = index * 6 / count;
  int section_start = count * section_num / 6;
  int section_offset = index - section_start;
  int next_section_start = count * (section_num + 1) / 6;
  int section_size = next_section_start - section_start;
  int partial_brightness = (section_offset * max_brightness) / section_size;

  switch(section_num) {
  case RAINBOW_SECTION_RED_TO_YELLOW:
    color.r = max_brightness;
    color.g = partial_brightness;
    break;
  case RAINBOW_SECTION_YELLOW_TO_GREEN:
    color.g = max_brightness;
    color.r = max_brightness - partial_brightness;
    break;
  case RAINBOW_SECTION_GREEN_TO_CYAN:
    color.g = max_brightness;
    color.b = partial_brightness;
    break;
  case RAINBOW_SECTION_CYAN_TO_BLUE:
    color.b = max_brightness;
    color.g = max_brightness - partial_brightness;
    break;
  case RAINBOW_SECTION_BLUE_TO_MAGENTA:
    color.b = max_brightness;
    color.r = partial_brightness;
    break;
  case RAINBOW_SECTION_MAGENTA_TO_RED:
    color.r = max_brightness;
    color.b = max_brightness - partial_brightness;
    break;
  }

  return color;
}

static void led_ring_animation_loop(void* param) {
  ESP_LOGI(LOG_LEDRING, "led_ring animation loop");
  led_ring_t ctx = (led_ring_t)param;

  while(1) {
    if(!ctx->animating) xSemaphoreTake(ctx->loop_semaphore, portMAX_DELAY);

    if(ctx->strobing) {
      ws2812rmt_set_colors(ctx->ws2812, ctx->led_color_buffer, 1, true);
    }

    if(ctx->spinning) {
      ws2812rmt_set_colors(ctx->ws2812, ctx->led_color_buffer, ctx->led_count, true);
    }

    if(ctx->animating) {
      vTaskDelay(pdMS_TO_TICKS(100));

      // Shift the pattern
      rgb_t led_0 = ctx->led_color_buffer[0];
      for(int i=0; i < ctx->led_count - 1; ++i) ctx->led_color_buffer[i] = ctx->led_color_buffer[i + 1];
      ctx->led_color_buffer[ctx->led_count - 1] = led_0;
    }
  }
}

led_ring_t led_ring_init(rmt_channel_t channel, gpio_num_t gpio_num, int led_count) {
  ESP_LOGI(LOG_LEDRING, "Initializing LED ring count %d, channel %d, GPIO %d", led_count, channel, gpio_num);
  led_ring_t ctx = led_rings + channel;
  ctx->led_count = led_count;
  ctx->led_color_buffer = calloc(led_count, sizeof(rgb_t));
  if(!ctx->led_color_buffer) return NULL;
  ctx->ws2812 = ws2812rmt_init(channel, gpio_num, led_count);
  ctx->animating = false;
  ctx->strobing = false;
  ctx->spinning = false;
  ctx->loop_semaphore = xSemaphoreCreateBinary();
  xTaskCreate(led_ring_animation_loop, "led_animation_loop", 2048, ctx, 5, &(ctx->loop_task));

  return ctx;
}

void led_ring_update(led_ring_t ctx) {
  ws2812rmt_set_colors(ctx->ws2812, ctx->led_color_buffer, ctx->led_count, true);
}

static void led_ring_start_loop(led_ring_t ctx) {
  ctx->animating = true;
  xSemaphoreGive(ctx->loop_semaphore);
}

void led_ring_start_spinner_loop(led_ring_t ctx) {
  ctx->spinning = true;
  led_ring_start_loop(ctx);
}

void led_ring_start_strobing_loop(led_ring_t ctx) {
  ctx->strobing = true;
  led_ring_start_loop(ctx);
}

void led_ring_stop_loop(led_ring_t ctx) {
  ctx->animating = false;
  ctx->strobing = false;
  ctx->spinning = false;
}

void led_ring_set_one_color(led_ring_t ctx, rgb_t color) {
  for (int i=0; i < ctx->led_count; ++i) ctx->led_color_buffer[i] = color;
}

void led_ring_set_colors(led_ring_t ctx, rgb_t* colors) {
  for(int i=0; i < ctx->led_count - 1; ++i) ctx->led_color_buffer[i] = colors[i];
}

void led_ring_set_pattern(led_ring_t ctx, rgb_t* pattern, int color_count) {
  for(int i=0; i < ctx->led_count; ++i) {
    int color_index = i % color_count;
    ctx->led_color_buffer[i] = pattern[color_index];
  }
}

void led_ring_set_rainbow(led_ring_t ctx, int max_brightness) {
  for (int i=0; i < ctx->led_count; ++i) {
    rgb_t color = led_ring_calculate_rainbow_color(i, ctx->led_count, max_brightness);
    ctx->led_color_buffer[i] = color;
  }
}

void led_ring_uninit(led_ring_t *ctx) {
  if(!ctx) return;
  vTaskDelete((*ctx)->loop_task);
  free((*ctx)->led_color_buffer);
  ws2812rmt_uninit(&((*ctx)->ws2812));
  ctx = NULL;
}

