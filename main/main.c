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

#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/rmt.h"
#include "ws2812rmt.h"
#include "string.h"
#include "coap_server.h"
#include "led_ring_resource.h"

esp_err_t event_handler(void *ctx, system_event_t *event)
{
  return ESP_OK;
}

#define WS2812_PIN      GPIO_NUM_14
#define WS2812_CHANNEL  RMT_CHANNEL_0

led_ring_t led_ring;

void app_main()
{
  nvs_flash_init();
  tcpip_adapter_init();
  ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
  ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
  wifi_config_t sta_config = {
      .sta = {
          .ssid = "ssid",
          .password = "password",
          .bssid_set = false
      }
  };
  ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
  ESP_ERROR_CHECK( esp_wifi_start() );
  ESP_ERROR_CHECK( esp_wifi_connect() );

  led_ring = led_ring_init(WS2812_CHANNEL, WS2812_PIN, 24);
  led_ring_set_rainbow(led_ring, 64);
  led_ring_start_strobing_loop(led_ring);

  coap_context_t* server = coap_server_create();
  led_ring_resource_init(server, led_ring);
  coap_server_start(server);
}
