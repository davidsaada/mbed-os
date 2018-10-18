/*
 * Copyright (c) 2018 ARM Limited. All rights reserved.
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
#ifndef _KVSTORE_STATIC_API
#define _KVSTORE_STATIC_API

#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _opaque_kv_key_iterator *kv_iterator_t;
typedef struct _opaque_kv_set_handle *kv_set_handle_t;

#define KV_WRITE_ONCE_FLAG        (1 << 0)
#define KV_ENCRYPT_FLAG           (1 << 1)
#define KV_AUTHENTICATE_FLAG      (1 << 2)
#define KV_ROLLBACK_PROTECT_FLAG  (1 << 3)

static const uint32_t KV_MAX_KEY_LENGTH = 128;

typedef struct info {
    size_t size;
    uint32_t flags;
} kv_info_t;

/**
 * @brief Set one KVStore item, given key and value.
 *
 * @param[in]  full_name_key        Partition_path/Key.
 * @param[in]  buffer               Value data buffer.
 * @param[in]  size                 Value data size.
 * @param[in]  create_flags         Flag mask.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_set(const char *full_name_key, const void *buffer, size_t size, uint32_t create_flags);

/**
 * @brief Get one KVStore item, given key.
 *
 * @param[in]  full_name_key        Partition_path/Key.
 * @param[in]  buffer               Value data buffer.
 * @param[in]  buffer_size          Value data buffer size.
 * @param[out] actual_size          Actual read size.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_get(const char *full_name_key, void *buffer, size_t buffer_size, size_t *actual_size);

/**
 * @brief Get information of a given key.
 *
 * @param[in]  full_name_key        Partition_path/Key.
 * @param[out] info                 Returned information structure.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_get_info(const char *full_name_key, kv_info_t *info);

/**
 * @brief Remove a KVStore item, given key.
 *
 * @param[in]  full_name_key        Partition_path/Key.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_remove(const char *full_name_key);

/**
 * @brief Start an incremental KVStore set sequence.
 *
 * @param[out] handle               Returned incremental set handle.
 * @param[in]  key                  Key.
 * @param[in]  final_data_size      Final value data size.
 * @param[in]  create_flags         Flag mask.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_set_start(kv_set_handle_t *handle, const char *key, size_t final_data_size, uint32_t create_flags);

/**
 * @brief Add data to incremental KVStore set sequence.
 *
 * @param[in]  handle               Incremental set handle.
 * @param[in]  value_data           value data to add.
 * @param[in]  data_size            value data size.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_set_add_data(kv_set_handle_t handle, const void *value_data, size_t data_size);

/**
 * @brief Finalize an incremental KVStore set sequence.
 *
 * @param[in]  handle               Incremental set handle.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_set_finalize(kv_set_handle_t handle);

/**
 * @brief Start an iteration over KVStore keys.
 *
 * @param[out] it                   Allocating iterator handle.
 *                                  Do not forget to call kv_iterator_close
 *                                  to deallocate the memory.
 * @param[in]  full_prefix          Pratition/Key prefix.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_iterator_open(kv_iterator_t *it, const char *full_prefix = NULL);

/**
 * @brief Get next key in iteration.
 *
 * @param[in]  it                   Iterator handle.
 * @param[in]  key                  Buffer for returned key.
 * @param[in]  key_size             Key buffer size.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_iterator_next(kv_iterator_t it, char *key, size_t key_size);

/**
 * @brief Close iteration and deallocating the iterator handle.
 *
 * @param[in]  it                   Iterator handle.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_iterator_close(kv_iterator_t it);

//Helper function
//Currently for test only. please dont use it cause it might be removed before release
int kv_reset(const char * kvstore_path);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif
#endif
