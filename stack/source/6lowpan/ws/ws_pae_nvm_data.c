/*
 * Copyright (c) 2019-2021, Pelion and affiliates.
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

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "stack-services/ns_list.h"
#include "stack-services/ns_trace.h"
#include "stack-services/common_functions.h"

#include "nwk_interface/protocol.h"
#include "security/protocols/sec_prot_certs.h"
#include "security/protocols/sec_prot_keys.h"
#include "6lowpan/ws/ws_config.h"
#include "6lowpan/ws/ws_pae_nvm_store.h"
#include "6lowpan/ws/ws_pae_controller.h"
#include "6lowpan/ws/ws_pae_time.h"

#include "6lowpan/ws/ws_pae_nvm_data.h"

#define TRACE_GROUP "wsnv"

#define PAE_NVM_FIELD_NOT_SET            0   // Field is not present
#define PAE_NVM_FIELD_SET                1   // Field is present

void ws_pae_nvm_store_keys_tlv_create(keys_nvm_tlv_t *tlv_entry, sec_prot_keys_t *sec_keys)
{
    tlv_entry->tag = PAE_NVM_KEYS_TAG;
    tlv_entry->len = PAE_NVM_KEYS_LEN;

    uint8_t *tlv = (uint8_t *) &tlv_entry->data[0];

    uint8_t *eui_64 = sec_prot_keys_ptk_eui_64_get(sec_keys);
    if (eui_64) {
        *tlv++ = PAE_NVM_FIELD_SET;
        memcpy(tlv, eui_64, 8);
    } else {
        *tlv++ = PAE_NVM_FIELD_NOT_SET;
        memset(tlv, 0, 8);
    }
    tlv += 8;

    uint8_t *pmk = sec_prot_keys_pmk_get(sec_keys);
    if (pmk) {
        *tlv++ = PAE_NVM_FIELD_SET;
        uint32_t lifetime = sec_prot_keys_pmk_lifetime_get(sec_keys);
        tlv = common_write_32_bit(lifetime, tlv);
        memcpy(tlv, pmk, PMK_LEN);
    } else {
        *tlv++ = PAE_NVM_FIELD_NOT_SET;
        memset(tlv, 0, 4 + PMK_LEN);
    }
    tlv += PMK_LEN;

    uint64_t counter = sec_prot_keys_pmk_replay_cnt_get(sec_keys);
    tlv = common_write_64_bit(counter, tlv);

    uint8_t *ptk = sec_prot_keys_ptk_get(sec_keys);
    if (ptk) {
        *tlv++ = PAE_NVM_FIELD_SET;
        uint32_t lifetime = sec_prot_keys_ptk_lifetime_get(sec_keys);
        tlv = common_write_32_bit(lifetime, tlv);
        memcpy(tlv, ptk, PTK_LEN);
    } else {
        *tlv++ = PAE_NVM_FIELD_NOT_SET;
        memset(tlv, 0, 4 + PTK_LEN);
    }
    tlv += PTK_LEN;

    tr_info("NVM KEYS write");
}

int8_t ws_pae_nvm_store_keys_tlv_read(keys_nvm_tlv_t *tlv_entry, sec_prot_keys_t *sec_keys)
{
    if (!tlv_entry || !sec_keys) {
        return -1;
    }

    if (tlv_entry->tag != PAE_NVM_KEYS_TAG || tlv_entry->len != PAE_NVM_KEYS_LEN) {
        return -1;
    }

    uint8_t *tlv = (uint8_t *) &tlv_entry->data[0];

    // EUI-64 set */
    if (*tlv++ == PAE_NVM_FIELD_SET) {
        sec_prot_keys_ptk_eui_64_write(sec_keys, tlv);
    }
    tlv += 8;

    // PMK set
    if (*tlv++ == PAE_NVM_FIELD_SET) {
        uint32_t lifetime = common_read_32_bit(tlv);
        tlv += 4;
        sec_prot_keys_pmk_write(sec_keys, tlv, lifetime);
    } else {
        tlv += 4;
    }
    tlv += PMK_LEN;

    uint64_t counter = common_read_64_bit(tlv);
    tlv += 8;
    sec_prot_keys_pmk_replay_cnt_set(sec_keys, counter);

    // PTK set
    if (*tlv++ == PAE_NVM_FIELD_SET) {
        uint32_t lifetime = common_read_32_bit(tlv);
        tlv += 4;
        sec_prot_keys_ptk_write(sec_keys, tlv, lifetime);
    } else {
        tlv += 4;
    }

    tlv += PTK_LEN;

    sec_prot_keys_updated_reset(sec_keys);

    tr_info("NVM KEYS read");

    return 0;
}

static uint8_t *ws_pae_nvm_store_frame_counter_tlv_create_counter(uint8_t *tlv,
                                                           frame_counters_t *counters)
{
    for (uint8_t index = 0; index < GTK_NUM; index++) {
        if (!counters->counter[index].set) {
            *tlv++ = PAE_NVM_FIELD_NOT_SET;
            memset(tlv, 0, GTK_LEN + 4 + 4);
            tlv += GTK_LEN + 4 + 4;
            continue;
        }
        *tlv++ = PAE_NVM_FIELD_SET;
        memcpy(tlv, counters->counter[index].gtk, GTK_LEN);
        tlv += GTK_LEN;
        tlv = common_write_32_bit(counters->counter[index].frame_counter, tlv);
        tlv = common_write_32_bit(counters->counter[index].max_frame_counter_chg, tlv);
    }
    return tlv;
}

void ws_pae_nvm_store_frame_counter_tlv_create(frame_cnt_nvm_tlv_t *tlv_entry,
                                               uint32_t restart_cnt,
                                               uint16_t pan_version,
                                               uint16_t lpan_version,
                                               frame_counters_t *gtk_counters,
                                               frame_counters_t *lgtk_counters,
                                               uint64_t stored_time)
{
    tlv_entry->tag = PAE_NVM_FRAME_COUNTER_TAG;
    tlv_entry->len = PAE_NVM_FRAME_COUNTER_LEN;

    uint8_t *tlv = (uint8_t *) &tlv_entry->data[0];

    tlv = common_write_32_bit(restart_cnt, tlv);
    tlv = common_write_64_bit(stored_time, tlv);
    tlv = common_write_16_bit(pan_version, tlv);
    tlv = common_write_16_bit(lpan_version, tlv);
    tlv = ws_pae_nvm_store_frame_counter_tlv_create_counter(tlv, gtk_counters);
    tlv = ws_pae_nvm_store_frame_counter_tlv_create_counter(tlv, lgtk_counters);

    tr_info("NVM FRAME COUNTER write; stored time: %"PRIu64, stored_time);
}

static uint8_t *ws_pae_nvm_store_frame_counter_tlv_read_counter(uint8_t *tlv,
                                                                frame_counters_t *counters)
{
    for (uint8_t index = 0; index < GTK_NUM; index++) {
        // Frame counter not set
        if (*tlv++ == PAE_NVM_FIELD_NOT_SET) {
            counters->counter[index].set = false;
            tlv += GTK_LEN + 4 + 4;
            continue;
        }
        // Frame counter is set, read GTK key and counter values
        counters->counter[index].set = true;
        memcpy(counters->counter[index].gtk, tlv, GTK_LEN);
        tlv += GTK_LEN;
        counters->counter[index].frame_counter = common_read_32_bit(tlv);
        tlv += 4;
        counters->counter[index].max_frame_counter_chg = common_read_32_bit(tlv);
        tlv += 4;
    }
    return tlv;
}

int8_t ws_pae_nvm_store_frame_counter_tlv_read(frame_cnt_nvm_tlv_t *tlv_entry,
                                               uint32_t *restart_cnt,
                                               uint64_t *stored_time,
                                               uint16_t *pan_version,
                                               uint16_t *lpan_version,
                                               frame_counters_t *gtk_counters,
                                               frame_counters_t *lgtk_counters)
{
    if (!tlv_entry || !gtk_counters || !lgtk_counters) {
        return -1;
    }

    if (tlv_entry->tag != PAE_NVM_FRAME_COUNTER_TAG || tlv_entry->len != PAE_NVM_FRAME_COUNTER_LEN) {
        return -1;
    }

    uint8_t *tlv = (uint8_t *) &tlv_entry->data[0];

    *restart_cnt = common_read_32_bit(tlv);
    tlv += 4;

    *stored_time = common_read_64_bit(tlv);
    tlv += 8;

    *pan_version = common_read_16_bit(tlv);
    tlv += 2;

    *lpan_version = common_read_16_bit(tlv);
    tlv += 2;

    tlv = ws_pae_nvm_store_frame_counter_tlv_read_counter(tlv, gtk_counters);
    tlv = ws_pae_nvm_store_frame_counter_tlv_read_counter(tlv, lgtk_counters);

    tr_info("NVM FRAME COUNTER read; stored time: %"PRIu64, *stored_time);

    return 0;
}

void ws_pae_nvm_store_key_storage_index_tlv_create(nvm_tlv_t *tlv_entry, uint64_t bitfield)
{
    tlv_entry->tag = PAE_NVM_KEY_STORAGE_INDEX_TAG;
    tlv_entry->len = PAE_NVM_KEY_STORAGE_INDEX_LEN;

    uint8_t *tlv = ((uint8_t *) &tlv_entry->tag) + NVM_TLV_FIXED_LEN;

    tlv = common_write_64_bit(bitfield, tlv);

    tr_info("NVM KEY STORAGE INDEX write");
}

int8_t ws_pae_nvm_store_key_storage_index_tlv_read(nvm_tlv_t *tlv_entry, uint64_t *bitfield)
{
    if (!tlv_entry || !bitfield) {
        return -1;
    }

    if (tlv_entry->tag != PAE_NVM_KEY_STORAGE_INDEX_TAG || tlv_entry->len != PAE_NVM_KEY_STORAGE_INDEX_LEN) {
        return -1;
    }

    uint8_t *tlv = ((uint8_t *) &tlv_entry->tag) + NVM_TLV_FIXED_LEN;
    *bitfield = common_read_64_bit(tlv);
    tlv += 8;

    tr_info("NVM KEY STORAGE INDEX read");

    return 0;
}

void ws_pae_nvm_store_key_storage_tlv_create(nvm_tlv_t *tlv_entry, uint16_t length)
{
    memset(tlv_entry, 0, sizeof(key_storage_nvm_tlv_entry_t));

    tlv_entry->tag = PAE_NVM_KEY_STORAGE_TAG;
    tlv_entry->len = length - sizeof(nvm_tlv_t);

    tr_debug("NVM KEY STORAGE create");
}

int8_t ws_pae_nvm_store_key_storage_tlv_read(nvm_tlv_t *tlv_entry, uint16_t length)
{
    if (!tlv_entry || !length) {
        return -1;
    }

    if (tlv_entry->tag != PAE_NVM_KEY_STORAGE_TAG || tlv_entry->len != length - sizeof(nvm_tlv_t)) {
        return -1;
    }

    tr_debug("NVM KEY STORAGE read");

    return 0;
}


