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
#ifndef _KV_MAP
#define _KV_MAP

#include "KVStore.h"

// Attach and detach
int kv_init();
int kv_attach(const char *partition_name, mbed::KVStore *kv_instance);
int kv_detach(const char *partition_name);
int kv_deinit();

// Full name lookup and then break it into KVStore instance and key
int kv_lookup(const char *full_name, mbed::KVStore **kv_instance, char *key);

#endif
