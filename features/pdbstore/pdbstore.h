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

#ifndef _MBED_PDBSTORE_H_
#define _MBED_PDBSTORE_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PDBSTORE_SUCCESS                =  0,
    PDBSTORE_READ_ERROR             = -1,
    PDBSTORE_WRITE_ERROR            = -2,
    PDBSTORE_NOT_FOUND              = -3,
    PDBSTORE_DATA_CORRUPT           = -4,
    PDBSTORE_INVALID_ARGUMENT       = -5,
    PDBSTORE_KEY_IS_READONLY        = -6,
    PDBSTORE_MEDIA_FULL             = -7,
    PDBSTORE_INTERNAL_ERROR         = -8,
    PDBSTORE_NOT_INITIALIZED        = -9,
} PDBSTORE_status_e;

typedef struct {
    uint32_t bank_size;
    uint32_t start_offset;
    uint8_t  erase_val;
    void *bank_base;
    int (*read_func)(void * /* buffer */, uint32_t /*addr*/, uint32_t /* size */);
    int (*prog_func)(const void * /* buffer */, uint32_t /*addr*/, uint32_t /* size */);
    int (*erase_func)(uint32_t /*addr*/, uint32_t /* size */);
} pdbstore_bank_params_t;

#define PDBSTORE_MAX_BANKS 2

/* Dual bank case */
#define PDBSTORE_READONLY_BANK 0
#define PDBSTORE_WRITABLE_BANK 1

/* Single bank case */
#define PDBSTORE_READONLY_WRITABLE_BANK 0

#define PDBSTORE_MAX_KEY_SIZE  16
#define PDBSTORE_MAX_DATA_SIZE 1024

#define PDBSTORE_RESILIENT_FLAG 0x01

int pdbstore_init(int init_num_banks, pdbstore_bank_params_t *init_bank_params);

int pdbstore_deinit();

int pdbstore_reset();

int pdbstore_get(const char *key, void **data, size_t *actual_size);

int pdbstore_set(const char *key, const void *buffer, size_t buffer_size, uint8_t flags);

int pdbstore_remove(const char *key);

#ifdef __cplusplus
}
#endif

#endif // _MBED_PDBSTORE_H_
