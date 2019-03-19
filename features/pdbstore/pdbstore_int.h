/*
 * Copyright (c) 2019 ARM Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _MBED_PDBSTORE_INT_H_
#define _MBED_PDBSTORE_INT_H_

#ifdef _MBED_PDBSTORE_C_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FALSE 0
#define TRUE  1

typedef int boolean;

typedef enum {
    FIRST_AREA = 0,
    AREA_INDEX_READONLY = FIRST_AREA,
    AREA_INDEX_WRITABLE,
    AREA_INDEX_STAGING,
    NUM_AREAS
} area_index_e;

int pdbstore_init_readonly_area(int init_num_banks, pdbstore_bank_params_t *init_bank_params);
int pdbstore_write_record(area_index_e area, const char *key, const void *data, size_t data_size, uint8_t flags,
                          uint32_t offset, uint32_t *next_offset, boolean replace_current);

#ifdef __cplusplus
}
#endif

#endif // _MBED_PDBSTORE_C_
#endif // _MBED_PDBSTORE_INT_H_
