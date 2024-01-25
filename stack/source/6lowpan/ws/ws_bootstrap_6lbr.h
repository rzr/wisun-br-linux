/*
 * Copyright (c) 2021, Pelion and affiliates.
 * Copyright (c) 2021-2023 Silicon Laboratories Inc. (www.silabs.com)
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

#ifndef WS_BOOTSTRAP_6LBR_H_
#define WS_BOOTSTRAP_6LBR_H_
#include <stdint.h>
#include "common/log.h"
#include "common/events_scheduler.h"

struct net_if;
struct mcps_data_ind;
struct mcps_data_rx_ie_list;

void ws_bootstrap_6lbr_mngt_ind(struct net_if *cur, const struct mcps_data_ind *data,
                                const struct mcps_data_rx_ie_list *ie_ext, uint8_t message_type);
void ws_bootstrap_6lbr_asynch_confirm(struct net_if *interface, uint8_t asynch_message);
void ws_bootstrap_6lbr_init(struct net_if *cur);
void ws_bootstrap_6lbr_eapol_relay_socket_cb(int fd);
void ws_bootstrap_6lbr_eapol_auth_relay_socket_cb(int fd);

#endif
