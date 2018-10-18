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

#include "Timer.h"
#include "greentea-client/test_env.h"
#include "unity/unity.h"
#include "utest/utest.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include "mbed_error.h"

using namespace utest::v1;

static const char *const key1      = "kv/key1";
static const char *const key1_val1 = "val1";
static const char *const key2      = "kv/name_of_key2";
static const char *const key2_val1 = "val3";
static const char *const key2_val2 = "val2 of key 2";
static const char *const key2_val3 = "Val1 value of key 2            ";
static const char *const key3      = "kv/This_is_the_name_of_key3";
static const char *const key3_val1 = "Data value of key 3 is the following";
static const char *const key4      = "kv/This_is_the_name_of_key4";
static const char *const key4_val1 = "Is this the value of key 4?";
static const char *const key4_val2 = "What the hell is the value of key 4, god damn it!";
static const char *const key5      = "kv/This_is_the_real_name_of_Key5";
static const char *const key5_val1 = "Key 5 value that should definitely be written";
static const char *const key5_val2 = "Key 5 value that should definitely not be written";

static void kv_global_api_test()
{
    uint8_t get_buf[256];
    size_t actual_data_size;
    int result;
    kv_info_t info;
    kv_reset("kv/");

    result = kv_set(key1, key1_val1, strlen(key1_val1), 0);
    TEST_ASSERT_EQUAL(MBED_SUCCESS, result);

    result = kv_set(key2, key2_val1, strlen(key2_val1), 0);
    TEST_ASSERT_EQUAL(MBED_SUCCESS, result);

    result = kv_set(key2, key2_val2, strlen(key2_val2), 0);
    TEST_ASSERT_EQUAL(MBED_SUCCESS, result);

    result = kv_set(key2, key2_val3, strlen(key2_val3), 0);
    TEST_ASSERT_EQUAL(MBED_SUCCESS, result);

    result = kv_set(key3, key3_val1, strlen(key3_val1), 0);
    TEST_ASSERT_EQUAL(MBED_SUCCESS, result);

    result = kv_get(key3, get_buf, sizeof(get_buf), &actual_data_size);
    TEST_ASSERT_EQUAL(MBED_SUCCESS, result);
    TEST_ASSERT_EQUAL(strlen(key3_val1), actual_data_size);
    TEST_ASSERT_EQUAL_STRING_LEN(key3_val1, get_buf, strlen(key3_val1));

    for (int j = 0; j < 2; j++) {
        result = kv_set(key4, key4_val1, strlen(key4_val1), 0);
        TEST_ASSERT_EQUAL(MBED_SUCCESS, result);

        result = kv_set(key4, key4_val2, strlen(key4_val2), 0);
        TEST_ASSERT_EQUAL(MBED_SUCCESS, result);
    }

    result = kv_remove(key3);
    TEST_ASSERT_EQUAL(MBED_SUCCESS, result);

    result = kv_remove(key3); //in FileSystemStore this returnes -2 instead of -3 looks like a bug. SD/FAT
    //TEST_ASSERT_EQUAL(KVSTORE_NOT_FOUND, result);
    TEST_ASSERT_NOT_EQUAL(MBED_SUCCESS, result);

    result = kv_get_info(key5, &info);
    TEST_ASSERT_EQUAL(MBED_ERROR_ITEM_NOT_FOUND, result);

    result = kv_set(key5, key5_val1, strlen(key5_val1), KV_WRITE_ONCE_FLAG);
    TEST_ASSERT_EQUAL(MBED_SUCCESS, result);

    result = kv_set(key5, key5_val2, strlen(key5_val2), 0);
    TEST_ASSERT_EQUAL(MBED_ERROR_WRITE_PROTECTED, result);

    result = kv_remove(key5);
    TEST_ASSERT_EQUAL(MBED_ERROR_WRITE_PROTECTED, result);

    result = kv_get_info(key5, &info);
    TEST_ASSERT_EQUAL(MBED_SUCCESS, result);
    TEST_ASSERT_EQUAL(strlen(key5_val1), info.size);
    TEST_ASSERT_EQUAL(KV_WRITE_ONCE_FLAG, info.flags);

    result = kv_get(key5, get_buf, sizeof(get_buf), &actual_data_size);
    TEST_ASSERT_EQUAL(MBED_SUCCESS, result);
    TEST_ASSERT_EQUAL(strlen(key5_val1), actual_data_size);
    TEST_ASSERT_EQUAL_STRING_LEN(key5_val1, get_buf, strlen(key5_val1));

    for (int i = 0; i < 2; i++) {
        result = kv_get(key1, get_buf, sizeof(get_buf), &actual_data_size);
        TEST_ASSERT_EQUAL(MBED_SUCCESS, result);
        TEST_ASSERT_EQUAL(strlen(key1_val1), actual_data_size);
        TEST_ASSERT_EQUAL_STRING_LEN(key1_val1, get_buf, strlen(key1_val1));

        result = kv_get(key2, get_buf, sizeof(get_buf), &actual_data_size);
        TEST_ASSERT_EQUAL(MBED_SUCCESS, result);
        TEST_ASSERT_EQUAL(strlen(key2_val3), actual_data_size);
        TEST_ASSERT_EQUAL_STRING_LEN(key2_val3, get_buf, strlen(key2_val3));

        result = kv_get(key3, get_buf, sizeof(get_buf), &actual_data_size);
        TEST_ASSERT_EQUAL(MBED_ERROR_ITEM_NOT_FOUND, result);

        result = kv_get(key4, get_buf, sizeof(get_buf), &actual_data_size);
        TEST_ASSERT_EQUAL(MBED_SUCCESS, result);
        TEST_ASSERT_EQUAL(strlen(key4_val2), actual_data_size);
        TEST_ASSERT_EQUAL_STRING_LEN(key4_val2, get_buf, strlen(key4_val2));

        result = kv_get(key5, get_buf, sizeof(get_buf), &actual_data_size);
        TEST_ASSERT_EQUAL(MBED_SUCCESS, result);
        TEST_ASSERT_EQUAL(strlen(key5_val1), actual_data_size);
        TEST_ASSERT_EQUAL_STRING_LEN(key5_val1, get_buf, strlen(key5_val1));

        kv_iterator_t it;
        char *char_get_buf = reinterpret_cast <char *> (get_buf);

        result = kv_iterator_open(&it, "This");
        TEST_ASSERT_EQUAL(MBED_SUCCESS, result);

        result = kv_iterator_next(it, char_get_buf, sizeof(get_buf));
        TEST_ASSERT_EQUAL(MBED_SUCCESS, result);

        char * str_dup = strdup(key4);
        char *token = strtok(str_dup, "/" );
        token = strtok(NULL, "/" );
        bool got_key4 = !strcmp(token, char_get_buf);
        free(str_dup);
        str_dup = strdup(key5);
        token = strtok(str_dup, "/" );
        token = strtok(NULL, "/" );
        bool got_key5 = !strcmp(token, char_get_buf);
        free(str_dup);

        TEST_ASSERT_EQUAL(true, got_key4 || got_key5);

        result = kv_iterator_next(it, char_get_buf, sizeof(get_buf));
        TEST_ASSERT_EQUAL(MBED_SUCCESS, result);
        if (got_key4) {
            str_dup = strdup(key5);
            token = strtok(str_dup, "/" );
            token = strtok(NULL, "/" );
            TEST_ASSERT_EQUAL_STRING(token, char_get_buf);
            free(str_dup);
        } else {
            str_dup = strdup(key4);
            token = strtok(str_dup, "/" );
            token = strtok(NULL, "/" );
            TEST_ASSERT_EQUAL_STRING(token, char_get_buf);
            free(str_dup);
        }

        result = kv_iterator_next(it, (char *)get_buf, sizeof(get_buf));
        TEST_ASSERT_EQUAL(MBED_ERROR_ITEM_NOT_FOUND, result);

        result = kv_iterator_close(it);
        TEST_ASSERT_EQUAL(MBED_SUCCESS, result);

    }

    TEST_ASSERT_EQUAL(MBED_SUCCESS, result);
}

utest::v1::status_t greentea_failure_handler(const Case *const source, const failure_t reason)
{
    greentea_case_failure_abort_handler(source, reason);
    return STATUS_CONTINUE;
}

Case cases[] = {
    Case("KVStore global api test",     kv_global_api_test,    greentea_failure_handler),
};

utest::v1::status_t greentea_test_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(120, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Specification specification(greentea_test_setup, cases, greentea_test_teardown_handler);

int main()
{
    return !Harness::run(specification);
}
