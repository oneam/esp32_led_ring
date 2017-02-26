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

#ifndef MAIN_LED_RING_RESOURCE_H_
#define MAIN_LED_RING_RESOURCE_H_

#include "led_ring.h"

#include <coap.h>

coap_resource_t* led_ring_resource_init(coap_context_t* ctx, led_ring_t led_ring);

#endif /* MAIN_LED_RING_RESOURCE_H_ */
