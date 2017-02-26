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

#include "led_ring_resource.h"

#include <cJSON.h>
#include <esp_log.h>
#include <resource.h>
#include <string.h>

const static char* resource_name = "led_ring";

const static char* mode_solid_color = "solid_color";
const static char* mode_static_rainbow = "static_rainbow";
const static char* mode_strobing_rainbow = "strobing_rainbow";
const static char* mode_spinning_rainbow = "spinning_rainbow";
const static char* mode_spinning_dots = "spinning_dots";
const static char* mode_strobing_dots = "strobing_dots";
const static char* mode_static_dots = "static_dots";

static char mode[256];
static rgb_t solid_color = { 0, 0, 0 };

const static rgb_t dots[] = {
    {64, 64, 64},
    {0, 0, 0},
    {0, 0, 0},
};

const static int dot_count = 3;
static led_ring_t led_ring;


/* GET handler */
static void led_ring_get_handler(coap_context_t *ctx, struct coap_resource_t *resource,
    const coap_endpoint_t *local_interface, coap_address_t *peer,
    coap_pdu_t *request, str *token, coap_pdu_t *response)
{
  char message[256];
  unsigned char buf[3];
  unsigned int len;

  response->hdr->code = COAP_RESPONSE_CODE(205);

  if (strcmp(mode, mode_solid_color) == 0) {
    sprintf(message, "[\"%s\", %d, %d, %d]", mode, solid_color.r, solid_color.g, solid_color.b);
  } else {
    sprintf(message, "[\"%s\"]", mode);
  }

  len = coap_encode_var_bytes(buf, COAP_MEDIATYPE_APPLICATION_JSON);
  coap_add_option(response, COAP_OPTION_CONTENT_TYPE, len, buf);

  len = coap_encode_var_bytes(buf, 5); // 5 second cache
  coap_add_option(response, COAP_OPTION_MAXAGE, len, buf);

  coap_add_data(response, strlen(message), (uint8_t*)message);
}

/* PUT handler */
static void led_ring_put_handler(coap_context_t *ctx, struct coap_resource_t *resource,
    const coap_endpoint_t *local_interface, coap_address_t *peer,
    coap_pdu_t *request, str *token, coap_pdu_t *response)
{
  cJSON* message = cJSON_Parse((const char *)request->data);
  cJSON* mode_json = cJSON_GetArrayItem(message, 0);

  if(!mode_json || mode_json->type != cJSON_String) goto error;

  if(strcmp(mode_json->valuestring, mode_solid_color) == 0) {

    cJSON* r_json = cJSON_GetArrayItem(message, 1);
    if(!r_json || r_json->type != cJSON_Number) goto error;

    cJSON* g_json = cJSON_GetArrayItem(message, 2);
    if(!g_json || g_json->type != cJSON_Number) goto error;

    cJSON* b_json = cJSON_GetArrayItem(message, 3);
    if(!b_json || b_json->type != cJSON_Number) goto error;

    led_ring_stop_loop(led_ring);

    solid_color.r = (uint8_t)r_json->valueint;
    solid_color.g = (uint8_t)g_json->valueint;
    solid_color.b = (uint8_t)b_json->valueint;

    led_ring_set_one_color(led_ring, solid_color);
  } else if(strcmp(mode_json->valuestring, mode_static_rainbow) == 0) {
    led_ring_stop_loop(led_ring);
    led_ring_set_rainbow(led_ring, 64);
  } else if(strcmp(mode_json->valuestring, mode_spinning_rainbow) == 0) {
    led_ring_stop_loop(led_ring);
    led_ring_set_rainbow(led_ring, 64);
    led_ring_start_spinner_loop(led_ring);
  } else if(strcmp(mode_json->valuestring, mode_strobing_rainbow) == 0) {
    led_ring_stop_loop(led_ring);
    led_ring_set_rainbow(led_ring, 64);
    led_ring_start_strobing_loop(led_ring);
  } else if(strcmp(mode_json->valuestring, mode_static_dots) == 0) {
    led_ring_stop_loop(led_ring);
    led_ring_set_pattern(led_ring, (rgb_t*)dots, dot_count);
  } else if(strcmp(mode_json->valuestring, mode_spinning_dots) == 0) {
    led_ring_stop_loop(led_ring);
    led_ring_set_pattern(led_ring, (rgb_t*)dots, dot_count);
    led_ring_start_spinner_loop(led_ring);
  } else if(strcmp(mode_json->valuestring, mode_strobing_dots) == 0) {
    led_ring_stop_loop(led_ring);
    led_ring_set_pattern(led_ring, (rgb_t*)dots, dot_count);
    led_ring_start_strobing_loop(led_ring);
  } else {
    goto error;
  }

  strcpy(mode, mode_json->valuestring);
  response->hdr->code = COAP_RESPONSE_CODE(204);
  cJSON_Delete(message);
  return;

  error:
  response->hdr->code = COAP_RESPONSE_CODE(400);
  cJSON_Delete(message);
}

coap_resource_t* led_ring_resource_init(coap_context_t* ctx, led_ring_t led_ring_ctx) {
  coap_resource_t* resource = coap_resource_init((uint8_t*)resource_name, strlen(resource_name), 0);
  if (!resource) return resource;

  led_ring = led_ring_ctx;
  strcpy(mode, mode_strobing_rainbow);

  coap_register_handler(resource, COAP_REQUEST_GET, led_ring_get_handler);
  coap_register_handler(resource, COAP_REQUEST_PUT, led_ring_put_handler);
  coap_add_resource(ctx, resource);

  return resource;
}
