/*
 * Copyright (c) 2014-2017, Pelion and affiliates.
 * SPDX-License-Identifier: Apache-2.0
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

#ifndef PROTOCOL_TIMER_H_
#define PROTOCOL_TIMER_H_

#define PROTOCOL_TIMER_PERIOD_MS 50

typedef enum {
    PROTOCOL_TIMER_STACK_TIM,
    PROTOCOL_TIMER_MAX
} protocol_timer_id_e;

typedef struct {
    void (*fptr)(uint16_t);
    uint16_t ticks;
    uint16_t orderedTime;
    uint16_t time_drifts;
} protocol_timer_t;

void protocol_timer_cb(int ticks);
void protocol_timer_start(protocol_timer_id_e id, void (*passed_fptr)(uint16_t), uint32_t time_ms);
void protocol_timer_stop(protocol_timer_id_e id);

#endif
