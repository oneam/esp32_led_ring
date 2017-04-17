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

#include "coap_server.h"

#include <coap/pdu.h>
#include <stddef.h>
#include <esp_log.h>
#include <freertos/task.h>
#include <lwip/sockets.h>

const static char *LOG_TAG = "CoAP_server";

#define COAP_DEFAULT_TIME_SEC 5
#define COAP_DEFAULT_TIME_USEC 0

#define COAP_INADDR_ALL_NODES ((u32_t)0xBB0100E0UL)

coap_context_t* coap_server_create() {
  coap_context_t*  ctx = NULL;
  coap_address_t   serv_addr;

   /* Prepare the CoAP server socket */
  coap_address_init(&serv_addr);
  serv_addr.addr.sin.sin_family      = AF_INET;
  serv_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;
  serv_addr.addr.sin.sin_port        = htons(COAP_DEFAULT_PORT);

  ctx = coap_new_context(&serv_addr);

  return ctx;
}

int coap_join_multicast(coap_context_t* ctx) {
  ESP_LOGI(LOG_TAG, "Joining All CoAP Nodes multicast group");

  ip_mreq mreq;
  mreq.imr_interface.s_addr = INADDR_ANY;
  mreq.imr_multiaddr.s_addr = COAP_INADDR_ALL_NODES;

  int result = setsockopt(ctx->sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
  if ( result < 0 ) ESP_LOGE(LOG_TAG, "coap_join_multicast: setsockopt returned %d", result);

  return result;
}

/**
 * Start the CoAP server.
 *
 * All resource should be registered before calling this function.
 * This server implementation only supports synchronous responses.
 *
 */
void coap_server_loop(void *param) {
  ESP_LOGI(LOG_TAG, "Starting CoAP server");
  coap_context_t* ctx = (coap_context_t*)param;

  if ( coap_join_multicast(ctx) < 0 ) goto end;

  for (;;) {
      coap_read(ctx);
  }

  end:
  coap_free_context(ctx);
}

void coap_server_start(coap_context_t* ctx) {
  xTaskCreate(coap_server_loop, "coap_server_loop", 2048, ctx, 10, NULL);
}
