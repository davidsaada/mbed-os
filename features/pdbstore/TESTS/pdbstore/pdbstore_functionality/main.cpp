
/* mbed Microcontroller Library
 * Copyright (c) 2017 ARM Limited
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

#define _MBED_PDBSTORE_C_ 1

#include "mbed.h"
#include "utest/utest.h"
#include "utest/utest_serial.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"
#include "pdbstore.h"
#include "pdbstore_int.h"
#include "unity.h"
#include <string.h>
#include <stdlib.h>

using namespace utest::v1;

#define FLASH_SIZE  (4 * 1024)
#define EEPROM_SIZE (6 * 1024)
#define ERASE_VAL 0xFF

static uint8_t *flash_base = 0;
static uint8_t *eeprom_base = 0;

static uint32_t curr_eeprom_write_addr;
static bool disable_eeprom_erase = FALSE;

int flash_read(void *buffer, uint32_t addr, uint32_t size)
{
    if (!size || (addr + size > FLASH_SIZE)) {
        return -1;
    }
    memcpy(buffer, flash_base + addr, size);
    return 0;
}

int flash_prog(const void *buffer, uint32_t addr, uint32_t size)
{
    if (!size || (addr + size > FLASH_SIZE)) {
        return -1;
    }
    for (uint32_t i = 0; i < size; i++) {
        if (flash_base[addr + i] != ERASE_VAL) {
            return -1;
        }
    }
    memcpy(flash_base + addr, buffer, size);
    return 0;
}

int flash_erase(uint32_t addr, uint32_t size)
{
    if (!size || (addr + size > FLASH_SIZE)) {
        return -1;
    }
    memset(flash_base + addr, ERASE_VAL, size);
    return 0;
}


int eeprom_read(void *buffer, uint32_t addr, uint32_t size)
{
    if (!size || (addr + size > EEPROM_SIZE)) {
        return -1;
    }
    memcpy(buffer, eeprom_base + addr, size);
    return 0;
}

int eeprom_prog(const void *buffer, uint32_t addr, uint32_t size)
{
    if (!size || (addr + size > EEPROM_SIZE)) {
        return -1;
    }

    for (uint32_t i = 0; i < size; i++) {
        if (eeprom_base[addr + i] != ERASE_VAL) {
            return -1;
        }
    }
    curr_eeprom_write_addr = addr + size;
    memcpy(eeprom_base + addr, buffer, size);
    return 0;
}

int eeprom_erase(uint32_t addr, uint32_t size)
{
    if (!size || (addr + size > EEPROM_SIZE)) {
        return -1;
    }
    if (disable_eeprom_erase) {
        return 0;
    }
    memset(eeprom_base + addr, ERASE_VAL, size);
    return 0;
}

const char *key1      = "key1";
const char *key1_val1 = "key1 val1";
const char *key2      = "k2";
const char *key2_val1 = "This is key2 value";
const char *key3      = "keyy3";
const char *key3_val1 = "What's the value of key 3?";
const char *key4      = "name4";
const char *key4_val1 = "1st value of key4 is";
const char *key4_val2 = "second value of key4";
const char *key5      = "key5";
const char *key5_val1 = "key5_val1                     ";
const char *key5_val2 = "?!#@*:$^;................%%%%%";
const char *key5_val3 = "Key 5 has the following value.";
const char *key6      = "kk6";
const char *key6_val1 = "Base value of key6 before appending the number is: ";

template <int num_banks>
void pdbstore_functionality_test()
{
    int curr_bank = 0;
    pdbstore_bank_params_t bank_params[PDBSTORE_MAX_BANKS];
    int ret;
    char set_buf[256];
    void *read_buf;
    uint32_t offset, next_offset;
    size_t actual_size;
    uint32_t key_size, data_size, ovh_size;
    uint32_t eeprom_writable_area_size;

    flash_base  = (uint8_t *) malloc(FLASH_SIZE);
    TEST_ASSERT_NOT_EQUAL(0, flash_base);

    eeprom_base = (uint8_t *) malloc(EEPROM_SIZE);
    TEST_ASSERT_NOT_EQUAL(0, eeprom_base);

    if (num_banks == 2) {
        // First bank - flash
        bank_params[curr_bank].read_func    = flash_read;
        bank_params[curr_bank].bank_size    = FLASH_SIZE;
        bank_params[curr_bank].bank_base    = flash_base;
        bank_params[curr_bank].start_offset = 1024;
        // Following params are just required for filling the readonly area by the test code
        bank_params[curr_bank].prog_func    = flash_prog;
        bank_params[curr_bank].erase_func   = flash_erase;
        bank_params[curr_bank].erase_val    = ERASE_VAL;

        ret = flash_erase(bank_params[curr_bank].start_offset, FLASH_SIZE - bank_params[curr_bank].start_offset);
        TEST_ASSERT_EQUAL(0, ret);

        curr_bank++;
    }

    ret = eeprom_erase(0, EEPROM_SIZE);
    TEST_ASSERT_EQUAL(0, ret);

    // Writable bank - eeprom
    bank_params[curr_bank].read_func    = eeprom_read;
    bank_params[curr_bank].prog_func    = eeprom_prog;
    bank_params[curr_bank].erase_func   = eeprom_erase;
    bank_params[curr_bank].bank_size    = EEPROM_SIZE;
    bank_params[curr_bank].bank_base    = eeprom_base;
    bank_params[curr_bank].start_offset = 0;
    bank_params[curr_bank].erase_val    = ERASE_VAL;

    // Prepare readonly area

    ret = pdbstore_init_readonly_area(num_banks, bank_params);
    TEST_ASSERT_EQUAL(0, ret);

    offset = 0;
    // Master record - data is 3 (records), big endian
    set_buf[0] = 0;
    set_buf[1] = 3;
    ret = pdbstore_write_record(AREA_INDEX_READONLY, "PDBS", set_buf, sizeof(uint16_t), 0, offset, &next_offset, FALSE);
    TEST_ASSERT_EQUAL(0, ret);
    offset = next_offset;

    ret = pdbstore_write_record(AREA_INDEX_READONLY, key1, key1_val1, strlen(key1_val1), 0, offset, &next_offset, FALSE);
    TEST_ASSERT_EQUAL(0, ret);
    offset = next_offset;

    ret = pdbstore_write_record(AREA_INDEX_READONLY, key2, key2_val1, strlen(key2_val1), 0, offset, &next_offset, FALSE);
    TEST_ASSERT_EQUAL(0, ret);
    offset = next_offset;

    ret = pdbstore_write_record(AREA_INDEX_READONLY, key3, key3_val1, strlen(key3_val1), 0, offset, &next_offset, FALSE);
    TEST_ASSERT_EQUAL(0, ret);
    offset = next_offset;

    ret = pdbstore_init(num_banks, bank_params);
    TEST_ASSERT_EQUAL(0, ret);

    ret = pdbstore_reset();
    TEST_ASSERT_EQUAL(0, ret);

    ret = pdbstore_get(key4, &read_buf, &actual_size);
    TEST_ASSERT_EQUAL(PDBSTORE_NOT_FOUND, ret);

    ret = pdbstore_remove(key4);
    TEST_ASSERT_EQUAL(PDBSTORE_NOT_FOUND, ret);

    ret = pdbstore_remove(key2);
    TEST_ASSERT_EQUAL(PDBSTORE_KEY_IS_READONLY, ret);

    ret = pdbstore_set(key4, key4_val1, strlen(key4_val1), 0);
    TEST_ASSERT_EQUAL(0, ret);

    ret = pdbstore_set(key3, key3_val1, strlen(key3_val1), 0);
    TEST_ASSERT_EQUAL(PDBSTORE_KEY_IS_READONLY, ret);

    ret = pdbstore_set(key5, key5_val1, strlen(key5_val1), 0);
    TEST_ASSERT_EQUAL(0, ret);

    ret = pdbstore_set(key5, key5_val1, strlen(key5_val1) - 1, 0);
    TEST_ASSERT_EQUAL(PDBSTORE_INVALID_ARGUMENT, ret);

    ret = pdbstore_set(key5, key5_val2, strlen(key5_val2), 0);
    TEST_ASSERT_EQUAL(0, ret);

    ret = pdbstore_get(key1, &read_buf, &actual_size);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(strlen(key1_val1), actual_size);
    TEST_ASSERT_EQUAL(0, strncmp(key1_val1, (char *) read_buf, actual_size));

    ret = pdbstore_get(key3, &read_buf, &actual_size);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(strlen(key3_val1), actual_size);
    TEST_ASSERT_EQUAL(0, strncmp(key3_val1, (char *) read_buf, actual_size));

    ret = pdbstore_get(key5, &read_buf, &actual_size);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(strlen(key5_val2), actual_size);
    TEST_ASSERT_EQUAL(0, strncmp(key5_val2, (char *) read_buf, actual_size));

    ret = pdbstore_get(key4, &read_buf, &actual_size);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(strlen(key4_val1), actual_size);
    TEST_ASSERT_EQUAL(0, strncmp(key4_val1, (char *) read_buf, actual_size));

    ret = pdbstore_remove(key4);
    TEST_ASSERT_EQUAL(0, ret);

    ret = pdbstore_get(key4, &read_buf, &actual_size);
    TEST_ASSERT_EQUAL(PDBSTORE_NOT_FOUND, ret);

    ret = pdbstore_set(key5, key5_val3, strlen(key5_val3), PDBSTORE_RESILIENT_FLAG);
    TEST_ASSERT_EQUAL(0, ret);

    ret = pdbstore_get(key5, &read_buf, &actual_size);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(strlen(key5_val3), actual_size);
    TEST_ASSERT_EQUAL(0, strncmp(key5_val3, (char *) read_buf, actual_size));

    // temporarily disable eeprom erasing to keep staging area alive
    disable_eeprom_erase = TRUE;
    ret = pdbstore_set(key6, key6_val1, strlen(key6_val1), PDBSTORE_RESILIENT_FLAG);
    TEST_ASSERT_EQUAL(0, ret);
    disable_eeprom_erase = FALSE;

    // Manually cripple last written key in writable area
    eeprom_base[curr_eeprom_write_addr - 1]++;

    // Use this to find the end of writable area (for later)
    eeprom_writable_area_size = curr_eeprom_write_addr;
    while (eeprom_base[eeprom_writable_area_size] == ERASE_VAL) {
        eeprom_writable_area_size++;
    }

    ret = pdbstore_get(key6, &read_buf, &actual_size);
    TEST_ASSERT_EQUAL(PDBSTORE_DATA_CORRUPT, ret);

    ret = pdbstore_deinit();
    TEST_ASSERT_EQUAL(0, ret);

    ret = pdbstore_init(num_banks, bank_params);
    TEST_ASSERT_EQUAL(0, ret);

    ret = pdbstore_get(key6, &read_buf, &actual_size);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(strlen(key6_val1), actual_size);
    TEST_ASSERT_EQUAL(0, strncmp(key6_val1, (char *) read_buf, actual_size));

    ret = pdbstore_get(key2, &read_buf, &actual_size);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(strlen(key2_val1), actual_size);
    TEST_ASSERT_EQUAL(0, strncmp(key2_val1, (char *) read_buf, actual_size));

    ret = pdbstore_get(key5, &read_buf, &actual_size);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(strlen(key5_val3), actual_size);
    TEST_ASSERT_EQUAL(0, strncmp(key5_val3, (char *) read_buf, actual_size));

    for (int i = 0; i < 1024; i++) {
        uint32_t curr_pos = curr_eeprom_write_addr;
        memset(set_buf, 'A' + i, sizeof(set_buf));
        key_size = 1 + rand() % (PDBSTORE_MAX_KEY_SIZE - 1);
        set_buf[key_size] = '\0';
        data_size = sizeof(set_buf);
        ovh_size = 8;
        ret = pdbstore_set(set_buf, set_buf, data_size, 0);
        if (curr_pos + ovh_size + key_size + data_size > eeprom_writable_area_size) {
            TEST_ASSERT_EQUAL(PDBSTORE_MEDIA_FULL, ret);
            break;
        }
        TEST_ASSERT_EQUAL(0, ret);
        ret = pdbstore_get(set_buf, &read_buf, &actual_size);
        TEST_ASSERT_EQUAL(0, ret);
        TEST_ASSERT_EQUAL(data_size, actual_size);
        TEST_ASSERT_EQUAL(0, memcmp(set_buf, read_buf, actual_size));
    }

    ret = pdbstore_reset();
    TEST_ASSERT_EQUAL(0, ret);

    ret = pdbstore_get(key2, &read_buf, &actual_size);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(strlen(key2_val1), actual_size);
    TEST_ASSERT_EQUAL(0, strncmp(key2_val1, (char *) read_buf, actual_size));

    ret = pdbstore_get(key5, &read_buf, &actual_size);
    TEST_ASSERT_EQUAL(PDBSTORE_NOT_FOUND, ret);

    ret = pdbstore_deinit();
    TEST_ASSERT_EQUAL(0, ret);

    free(flash_base);
    free(eeprom_base);
}


Case cases[] = {
    Case("PDBStore - single bank", pdbstore_functionality_test<1>),
    Case("PDBStore - dual bank",   pdbstore_functionality_test<2>),
};

utest::v1::status_t greentea_test_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(120, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Specification specification(greentea_test_setup, cases, greentea_test_teardown_handler);

int main()
{
    Harness::run(specification);
}
