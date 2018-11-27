/* mbed Microcontroller Library
 * Copyright (c) 2018 ARM Limited
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
#include "kvstore_global_api.h"
#include "kv_config.h"
#include "Timer.h"
#include "greentea-client/test_env.h"
#include "mbed_error.h"
#include "unity.h"
#include "utest.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

using namespace mbed;

#define TEST_ASSERT_EQUAL_OR_ABORT(expected, actual) \
        TEST_ASSERT_EQUAL(expected, actual); \
        if ((int64_t)(expected) != (int64_t)(actual)) { \
            return CMD_STATUS_FAIL; \
        }

#define TEST_ASSERT_EQUAL_ERROR_CODE_OR_ABORT(expected, actual) \
        TEST_ASSERT_EQUAL_ERROR_CODE(expected, actual); \
        if ((int64_t)(expected) != (int64_t)(actual)) { \
            return CMD_STATUS_FAIL; \
        }


using namespace utest::v1;

const size_t num_keys = 128;

typedef enum {
    CMD_STATUS_PASS,
    CMD_STATUS_FAIL,
    CMD_STATUS_CONTINUE,
    CMD_STATUS_ERROR
} cmd_status_t;

typedef enum {
    TDB_EXTERNAL_CFG,
    FILESYSTEMSTORE_CFG,
    INTERNAL_CFG,
} config_type_t;

static config_type_t config_type;
static bool unsupported_config = false;

static const int heap_alloc_threshold_size = 4096;

int _storage_config_TDB_EXTERNAL();
int _storage_config_FILESYSTEM();
int _storage_config_TDB_INTERNAL();

static bool initialized = false;

// This will override the default storage config function. Reason is the fact we want to cover as many
// configurations as possible.
int kv_init_storage_config()
{
    if (initialized) {
        return MBED_SUCCESS;
    }

    initialized = true;

    if (config_type == TDB_EXTERNAL_CFG) {
        return _storage_config_TDB_EXTERNAL();
    }

    if (config_type == FILESYSTEMSTORE_CFG) {
        return _storage_config_FILESYSTEM();
    }

    if (config_type == INTERNAL_CFG) {
        return _storage_config_TDB_INTERNAL();
    }

    return MBED_SUCCESS;
}

static cmd_status_t test_init(const char *kv_desc, bool format)
{
    int result;

    if (!strcmp(kv_desc, "TDB-External")) {
        config_type = TDB_EXTERNAL_CFG;
    } else if (!strcmp(kv_desc, "File-System")) {
        config_type = FILESYSTEMSTORE_CFG;
    } else if (!strcmp(kv_desc, "Internal")) {
        config_type = INTERNAL_CFG;
    }
    uint8_t *dummy = new (std::nothrow) uint8_t[heap_alloc_threshold_size];
    if (!dummy) {
        printf("Not enough heap to run test - test skipped\n");
        return CMD_STATUS_CONTINUE;
    }
    delete[] dummy;

    // A dummy get info call, triggering the configuration init
    kv_info_t info;
    result = kv_get_info("nokey", &info);

    if ((result == MBED_ERROR_UNSUPPORTED) || (result == MBED_ERROR_INVALID_SIZE)) {
        printf("Skipping init on unsupported configuration\n");
        unsupported_config = true;
        return CMD_STATUS_CONTINUE;
    }

    TEST_ASSERT_EQUAL_ERROR_CODE_OR_ABORT(MBED_ERROR_ITEM_NOT_FOUND, result);

    if (format) {
        result = kv_reset("");
        TEST_ASSERT_EQUAL_ERROR_CODE(MBED_SUCCESS, result);
    }

    return CMD_STATUS_CONTINUE;
}

static cmd_status_t test_run(int iter_num, bool verify)
{
    char *key_buf;
    const char *key;
    uint8_t *get_buf, *set_buf;
    size_t key_size = 16;
    size_t data_size = 32;
    size_t actual_data_size;
    kv_info_t info;
    uint32_t flags;
    int result;
    int curr_iter, start_iter;
    int ind;

    uint8_t *dummy = new (std::nothrow) uint8_t[heap_alloc_threshold_size];
    if (!dummy) {
        printf("Not enough heap to run test - test skipped\n");
        return CMD_STATUS_CONTINUE;
    }
    delete[] dummy;

    if (unsupported_config) {
        printf("Skipping run on unsupported configuration\n");
        return CMD_STATUS_CONTINUE;
    }

    key_buf = new char[key_size];
    key = const_cast<const char *>(key_buf);
    get_buf = new uint8_t[data_size];
    set_buf = new uint8_t[data_size];

    if (verify) {
        start_iter = 0;
        printf("Verifying iteration %d\n", iter_num);
    } else {
        printf("Running write iteration %d\n", iter_num);
        start_iter = iter_num - 1;
    }

    for (;;) {
        for (curr_iter = start_iter; curr_iter < iter_num; curr_iter++) {
            for (ind = 0; ind < curr_iter; ind++) {
                sprintf(key_buf, "key%d", curr_iter * 256 + ind);
                memset(set_buf, curr_iter, data_size / 2);
                memset(set_buf + data_size / 2, ind, data_size / 2);
                bool exists = true;
                if ((ind % 3) == 1) {
                    result = kv_get_info(key, &info);
                    if ((result == MBED_ERROR_ITEM_NOT_FOUND) || (result == MBED_ERROR_AUTHENTICATION_FAILED) ||
                            (result == MBED_ERROR_RBP_AUTHENTICATION_FAILED)) {
                        exists = false;
                    } else {
                        TEST_ASSERT_EQUAL_ERROR_CODE_OR_ABORT(MBED_SUCCESS, result);
                    }
                }
                if (verify) {
                    if (!exists) {
                        continue;
                    }
                    result = kv_get(key, get_buf, data_size, &actual_data_size);
                    // Authentication && RBP authentication errors can well happen following a sudden reset
                    // (all other errors shouldn't)
                    if ((result != MBED_ERROR_AUTHENTICATION_FAILED) &&
                            (result != MBED_ERROR_RBP_AUTHENTICATION_FAILED)) {
                        TEST_ASSERT_EQUAL_ERROR_CODE_OR_ABORT(MBED_SUCCESS, result);
                        TEST_ASSERT_EQUAL_OR_ABORT(data_size, actual_data_size);
                        TEST_ASSERT_EQUAL_OR_ABORT(0, memcmp(set_buf, get_buf, data_size));
                    }
                } else {
                    if (exists && ((ind % 3) == 1)) {
                        result = kv_remove(key);
                    } else {
                        flags = KV_REQUIRE_CONFIDENTIALITY_FLAG |
                                KV_REQUIRE_REPLAY_PROTECTION_FLAG;
                        result = kv_set(key, set_buf, data_size, flags);
                    }
                    TEST_ASSERT_EQUAL_ERROR_CODE_OR_ABORT(MBED_SUCCESS, result);
                }
            }
        }
        if (verify) {
            break;
        }
    }

    delete[] key_buf;
    delete[] get_buf;
    delete[] set_buf;

    return CMD_STATUS_CONTINUE;
}

static cmd_status_t handle_command(const char *key, const char *value)
{
    int iter_num;
    cmd_status_t status;

    if (strcmp(key, "format") == 0) {
        printf("Formatting %s configuration\n", value);
        status = test_init(value, true);
        greentea_send_kv("format_done", 1);
        return status;

    } else if (strcmp(key, "init") == 0) {
        printf("Initializing %s configuration\n", value);
        status = test_init(value, false);
        greentea_send_kv("init_done", 1);
        return status;

    } else if (strcmp(key, "verify") == 0) {
        sscanf(value, "%d", &iter_num);
        status = test_run(iter_num, true);
        greentea_send_kv("verify_done", 1);
        return status;

    } else if (strcmp(key, "run") == 0) {
        sscanf(value, "%d", &iter_num);
        status = test_run(iter_num, false);
        return status;

    } else if (strcmp(key, "exit") == 0) {
        if (strcmp(value, "pass") != 0) {
            printf("Test failed\n");
            return CMD_STATUS_FAIL;
        }
        printf("Test passed\n");
        return CMD_STATUS_PASS;

    } else {
        return CMD_STATUS_ERROR;
    }
}

int main()
{
    GREENTEA_SETUP(2400, "kvstore_resilience");

    char key[10 + 1] = {};
    char value[128 + 1] = {};

    greentea_send_kv("start", 1);

    // Handshake with host
    cmd_status_t cmd_status = CMD_STATUS_CONTINUE;
    while (CMD_STATUS_CONTINUE == cmd_status) {
        memset(key, 0, sizeof(key));
        memset(value, 0, sizeof(value));
        greentea_parse_kv(key, value, sizeof(key) - 1, sizeof(value) - 1);
        cmd_status = handle_command(key, value);
    }

    GREENTEA_TESTSUITE_RESULT(CMD_STATUS_PASS == cmd_status);
}
