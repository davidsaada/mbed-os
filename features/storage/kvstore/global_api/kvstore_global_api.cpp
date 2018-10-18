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
#include "kvstore_global_api.h"

#include "kv_config.h"
#include "kv_map.h"
#include "KVStore.h"
#include "mbed_error.h"

using namespace mbed;

// iterator handle
struct _opaque_kv_set_handle {
    bool handle_is_open;
    KVStore *kvstore_intance;
    KVStore::set_handle_t *set_handle;
};

// iterator handle
struct _opaque_kv_key_iterator {
    bool iterator_is_open;
    KVStore *kvstore_intance;
    KVStore::iterator_t *iterator_handle;
};

int kv_set(const char *full_name_key, const void *buffer, size_t size, uint32_t create_flags)
{
    int ret = storage_configuration();
    if (MBED_SUCCESS != ret) {
        return ret;
    }

    KVStore *kv_instance = NULL;
    char key[KV_MAX_KEY_LENGTH];
    kv_lookup(full_name_key, &kv_instance, key);
    ret = kv_instance->set(key, buffer, size, create_flags);
    return ret;
}

int kv_get(const char *full_name_key, void *buffer, size_t buffer_size, size_t *actual_size)
{
    int ret = storage_configuration();
    if (MBED_SUCCESS != ret) {
        return ret;
    }

    KVStore *kv_instance = NULL;
    char key[KV_MAX_KEY_LENGTH];
    kv_lookup(full_name_key, &kv_instance, key);
    return kv_instance->get(key, buffer, buffer_size, actual_size);
}

int kv_get_info(const char *full_name_key, kv_info_t *info)
{
    int ret = storage_configuration();
    if (MBED_SUCCESS != ret) {
        return ret;
    }

    KVStore *kv_instance = NULL;
    char key[KV_MAX_KEY_LENGTH];
    kv_lookup(full_name_key, &kv_instance, key);
    KVStore::info_t inner_info;
    ret =  kv_instance->get_info(key, &inner_info);
    if (MBED_SUCCESS != ret) {
        return ret;
    }
    info->flags = inner_info.flags;
    info->size =  inner_info.size;
    return ret;
}

int kv_remove(const char *full_name_key)
{
    int ret = storage_configuration();
    if (MBED_SUCCESS != ret) {
        return ret;
    }

    KVStore *kv_instance = NULL;
    char key[KV_MAX_KEY_LENGTH];
    kv_lookup(full_name_key, &kv_instance, key);
    return kv_instance->remove(key);
}

int kv_set_start(kv_set_handle_t *handle, const char *key, size_t final_data_size, uint32_t create_flags)
{
    int ret = storage_configuration();
    if (MBED_SUCCESS != ret) {
        return ret;
    }

    (*handle) = new _opaque_kv_set_handle;
    if (*handle == NULL) {
        return MBED_ERROR_FAILED_OPERATION;
    }
    (*handle)->handle_is_open = false;

    KVStore *kv_instance = NULL;
    KVStore::set_handle_t *inner_handle = new KVStore::set_handle_t;
    char out_key[KV_MAX_KEY_LENGTH];
    kv_lookup(key, &kv_instance, out_key);
    ret = kv_instance->set_start(inner_handle, out_key, final_data_size, create_flags);
    if (MBED_SUCCESS != ret) {
        delete inner_handle;
        return ret;
    }

    (*handle)->set_handle = inner_handle;
    (*handle)->handle_is_open = true;

    return ret;
}

int kv_set_add_data(kv_set_handle_t handle, const void *value_data, size_t data_size)
{
    if (!handle->handle_is_open) {
       return MBED_ERROR_INVALID_ARGUMENT;
    }

    return handle->kvstore_intance->set_add_data(*handle->set_handle, value_data, data_size);
}

int kv_set_finalize(kv_set_handle_t handle)
{
    if (!handle->handle_is_open) {
        return MBED_ERROR_INVALID_ARGUMENT;
    }

    int ret = handle->kvstore_intance->set_finalize(*handle->set_handle);

    delete handle->set_handle;
    delete handle;

    return ret;
}

int kv_iterator_open(kv_iterator_t *it, const char *full_prefix)
{
    int ret = storage_configuration();
    if (MBED_SUCCESS != ret) {
        return ret;
    }

    (*it) = new _opaque_kv_key_iterator;
    if (*it == NULL) {
        return MBED_ERROR_FAILED_OPERATION;
    }
    (*it)->iterator_is_open = false;

    KVStore *kv_instance = NULL;
    char key[KV_MAX_KEY_LENGTH];
    kv_lookup(full_prefix, &kv_instance, key);
    (*it)->kvstore_intance = kv_instance;

    KVStore::iterator_t *inner_it = new KVStore::iterator_t;
    ret = kv_instance->iterator_open(inner_it, key);
    if (MBED_SUCCESS != ret) {
        delete inner_it;
        return ret;
    }

    (*it)->iterator_handle = inner_it;
    (*it)->iterator_is_open = true;
    return ret;

}

int kv_iterator_next(kv_iterator_t it, char *key, size_t key_size)
{
    if (!it->iterator_is_open) {
        return MBED_ERROR_INVALID_ARGUMENT;
    }

    return it->kvstore_intance->iterator_next(*it->iterator_handle, key, key_size);
}

int kv_iterator_close(kv_iterator_t it)
{
    if (!it->iterator_is_open) {
        return MBED_ERROR_INVALID_ARGUMENT;
    }

    int ret = it->kvstore_intance->iterator_close(*it->iterator_handle);

    delete it->iterator_handle;
    delete it;

    return ret;
}

int kv_reset(const char * kvstore_name)
{
    int ret = storage_configuration();
    if (MBED_SUCCESS != ret) {
        return ret;
    }

    KVStore *kv_instance = NULL;
    char key[KV_MAX_KEY_LENGTH];
    kv_lookup(kvstore_name, &kv_instance, key);
    return kv_instance->reset();
}

