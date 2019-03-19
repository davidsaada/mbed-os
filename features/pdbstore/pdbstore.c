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

#include "pdbstore.h"

#define _MBED_PDBSTORE_C_ 1
#include "pdbstore_int.h"

// TODO: Is this required for packed?
#include "mbed_toolchain.h"
#include <string.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

int num_banks;
pdbstore_bank_params_t bank_params[PDBSTORE_MAX_BANKS];

typedef struct {
    uint32_t address;
    uint32_t size;
    pdbstore_bank_params_t *bank_params;
} pdbstore_area_params_t;

typedef MBED_PACKED(struct)
{
    uint16_t data_size;
    uint8_t  key_size;
    uint8_t  flags;
    uint32_t crc;
}
record_header_t;

typedef MBED_PACKED(struct)
{
    /* Number of keys in the readonly area */
    uint16_t num_keys;
}
master_record_data_t;

#define INITIAL_CRC 0xFFFFFFFFL
#define DELETE_FLAG 0x80
#define SUPPORTED_USER_FLAGS PDBSTORE_RESILIENT_FLAG

static pdbstore_area_params_t area_params[NUM_AREAS];

static boolean initialized = FALSE;
static boolean big_endian = FALSE;
static uint32_t free_space_offset;

#define WORK_BUF_SIZE 16
uint8_t work_buf[WORK_BUF_SIZE];

static inline uint16_t conv_ton16(uint16_t val)
{
    if (big_endian) {
        return val;
    }
    return (val >> 8) | (val << 8);

}

static inline uint32_t conv_ton32(uint32_t val)
{
    if (big_endian) {
        return val;
    }
    return ((val >> 24) & 0xff)     |
           ((val <<  8) & 0xff0000) |
           ((val >>  8) & 0xff00)   |
           ((val << 24) & 0xff000000);

}

// CRC32 calculation. Supports "rolling" calculation (using the initial value).
static uint32_t calc_crc(uint32_t init_crc, uint32_t data_size, const void *data_buf)
{
    uint32_t i, j;
    uint32_t crc, mask;

    crc = init_crc;
    for (i = 0; i < data_size; i++) {
        crc = crc ^ (uint32_t)(((uint8_t *)data_buf)[i]);
        for (j = 0; j < 8; j++) {
            mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return crc;
}

static int is_valid_key(const char *key)
{
    if (!key || !strlen(key) || strlen(key) > PDBSTORE_MAX_KEY_SIZE) {
        return FALSE;
    }

    if (strpbrk(key, " */?:;\"|<>\\")) {
        return FALSE;
    }
    return TRUE;
}

static inline uint32_t record_size(const char *key, uint32_t data_size)
{
    return sizeof(record_header_t) + strlen(key) + data_size;
}

static inline uint32_t abs_address(area_index_e area, uint32_t offset)
{
    return area_params[area].bank_params->start_offset + area_params[area].address + offset;
}

static void *area_ptr_by_offset(area_index_e area, uint32_t offset)
{
    uint32_t address = abs_address(area, offset);
    uint8_t *base = (uint8_t *) area_params[area].bank_params->bank_base;
    return (void *)(base + address);
}

static int read_area(area_index_e area, uint32_t offset, uint32_t size, void *buf)
{
    void *ptr;
    if (offset + size > area_params[area].size) {
        return PDBSTORE_READ_ERROR;
    }
    ptr = area_ptr_by_offset(area, offset);
    memcpy(buf, ptr, size);
    return PDBSTORE_SUCCESS;
}

static int write_area(area_index_e area, uint32_t offset, uint32_t size, const void *buf)
{
    uint32_t address = abs_address(area, offset);
    int ret = area_params[area].bank_params->prog_func(buf, address, size);
    if (ret) {
        return PDBSTORE_WRITE_ERROR;
    }
    return PDBSTORE_SUCCESS;
}

static int erase_area(area_index_e area, uint32_t offset, uint32_t size)
{
    uint32_t address = abs_address(area, offset);
    int ret = area_params[area].bank_params->erase_func(address, size);
    if (ret) {
        return PDBSTORE_WRITE_ERROR;
    }
    return PDBSTORE_SUCCESS;
}

static int reset_area(area_index_e area, uint32_t offset)
{
    uint8_t blank_buf[WORK_BUF_SIZE];
    memset(blank_buf, area_params[area].bank_params->erase_val, WORK_BUF_SIZE);
    uint32_t erase_size = area_params[area].size - offset;

    uint32_t read_offset = offset, read_size = erase_size;
    while (read_size) {
        uint32_t chunk_size = MIN(read_size, WORK_BUF_SIZE);
        int ret = read_area(area, read_offset, chunk_size, work_buf);
        if (ret) {
            return ret;
        }
        if (memcmp(work_buf, blank_buf, chunk_size)) {
            break;
        }
        read_offset += chunk_size;
        read_size -= chunk_size;
    }

    if (!read_size) {
        return PDBSTORE_SUCCESS;
    }

    return erase_area(area, offset, erase_size);
}

static int read_record(area_index_e area, uint32_t offset, char *key,
                       void **data_ptr, size_t *actual_data_size,
                       boolean copy_key, boolean *totally_corrupt,
                       uint8_t *flags, uint32_t *next_offset)
{
    int ret;
    record_header_t header;
    uint32_t total_size, key_size, data_size;
    uint32_t crc = INITIAL_CRC;

    *totally_corrupt = FALSE;

    if (offset + sizeof(header) >= area_params[area].address + area_params[area].size) {
        *totally_corrupt = TRUE;
        return PDBSTORE_DATA_CORRUPT;
    }

    ret = read_area(area, offset, sizeof(header), &header);
    if (ret) {
        return ret;
    }
    offset += sizeof(header);

    key_size = header.key_size;
    data_size = conv_ton16(header.data_size);

    if (!key_size || key_size > PDBSTORE_MAX_KEY_SIZE || data_size > PDBSTORE_MAX_DATA_SIZE) {
        *totally_corrupt = TRUE;
        return PDBSTORE_DATA_CORRUPT;
    }

    *flags = header.flags;
    total_size = key_size + data_size;

    if (offset + total_size > area_params[area].address + area_params[area].size) {
        *totally_corrupt = TRUE;
        return PDBSTORE_DATA_CORRUPT;
    }

    // Calculate CRC on header (excluding CRC itself)
    crc = calc_crc(crc, sizeof(record_header_t) - sizeof(crc), &header);

    if (copy_key) {
        ret = read_area(area, offset, key_size, key);
        if (ret) {
            return PDBSTORE_READ_ERROR;
        }
        key[key_size] = '\0';
    } else if (!key) {
        key = area_ptr_by_offset(area, offset);
    }
    crc = calc_crc(crc, key_size, key);
    offset += key_size;

    *data_ptr = area_ptr_by_offset(area, offset);
    crc = calc_crc(crc, data_size, *data_ptr);
    offset += data_size;

    *actual_data_size = data_size;
    *next_offset = offset;

    if (conv_ton32(crc) != header.crc) {
        return PDBSTORE_DATA_CORRUPT;
    }

    return PDBSTORE_SUCCESS;
}

static int find_record(const char *key, void **data_ptr, size_t *actual_size,
                       uint8_t *flags, area_index_e *area, uint32_t *offset)
{
    int ret;
    uint32_t next_offset;
    boolean totally_corrupt;
    char read_key[PDBSTORE_MAX_KEY_SIZE];


    for (*area = AREA_INDEX_READONLY; *area <= AREA_INDEX_WRITABLE; (*area)++) {

        *offset = 0;
        while (*offset < area_params[*area].size) {
            if ((*area == AREA_INDEX_WRITABLE) && (*offset >= free_space_offset)) {
                break;
            }

            ret = read_record(*area, *offset, read_key, data_ptr, actual_size,
                              TRUE, &totally_corrupt, flags, &next_offset);
            if (ret != PDBSTORE_SUCCESS) {
                // TODO: How do we handle corrupt records here? Do we fix them? Do we move on to the next if not totally corrupt?
                return ret;
            }
            if (!strcmp(read_key, key)) {
                return PDBSTORE_SUCCESS;
            }
            *offset = next_offset;
        }
    }
    return PDBSTORE_NOT_FOUND;
}

// Keep this one global for testing sake
int pdbstore_write_record(area_index_e area, const char *key, const void *data, size_t data_size,
                          uint8_t flags, uint32_t offset, uint32_t *next_offset, boolean replace_current)
{
    int ret;
    record_header_t header;
    uint32_t header_start_offset = 0;
    uint8_t *header_ptr = (uint8_t *) &header;

    if (offset + record_size(key, data_size) > area_params[area].size) {
        return PDBSTORE_MEDIA_FULL;
    }

    if (replace_current) {
        // In case we replace current record, don't erase entire record,
        // but leave key size and data size fields, as they're not changed anyway.
        // Reason is that if the write after erase fails, we will be left with awkward size values,
        // without being able to reach the next records (i.e. this record will become totally corrupt).
        header_start_offset = (uint32_t) &header.flags - (uint32_t) &header;
        header_ptr += header_start_offset;
        offset += header_start_offset;
        ret = erase_area(area, offset, record_size(key, data_size) - header_start_offset);
        if (ret) {
            return ret;
        }
    }

    header.key_size = strlen(key);
    header.data_size = conv_ton16(data_size);
    header.flags = flags;

    // Calculate CRC on header (without CRC), key & data
    header.crc = calc_crc(INITIAL_CRC, sizeof(header) - sizeof(header.crc), &header);
    header.crc = calc_crc(header.crc, header.key_size, key);
    header.crc = calc_crc(header.crc, data_size, data);
    header.crc = conv_ton32(header.crc);

    ret = write_area(area, offset, sizeof(header) - header_start_offset, header_ptr);
    if (ret) {
        return ret;
    }
    offset += sizeof(header) - header_start_offset;

    ret = write_area(area, offset, header.key_size, key);
    if (ret) {
        return ret;
    }
    offset += header.key_size;

    if (data_size) {
        ret = write_area(area, offset, data_size, data);
        offset += data_size;
    }

    *next_offset = offset;
    return PDBSTORE_SUCCESS;
}

static int pdbstore_do_set(const char *key, const void *data, size_t data_size, uint8_t flags)
{
    size_t actual_size;
    uint32_t offset, next_offset;
    area_index_e area;
    void *data_ptr;
    int ret;
    uint8_t read_flags;
    boolean replace_current = FALSE;

    if (!is_valid_key(key) || data_size > PDBSTORE_MAX_DATA_SIZE) {
        return PDBSTORE_INVALID_ARGUMENT;
    }

    if (!initialized) {
        return PDBSTORE_NOT_INITIALIZED;
    }

    ret = find_record(key, &data_ptr, &actual_size, &read_flags, &area, &offset);

    if (ret == PDBSTORE_SUCCESS) {
        if (area == AREA_INDEX_READONLY) {
            return PDBSTORE_KEY_IS_READONLY;
        }
        if (flags & DELETE_FLAG) {
            data_size = actual_size;
            data = data_ptr;
        } else if (data_size != actual_size) {
            return PDBSTORE_INVALID_ARGUMENT;
        }
        replace_current = TRUE;
    } else if (ret == PDBSTORE_NOT_FOUND) {
        if (flags & DELETE_FLAG) {
            return PDBSTORE_NOT_FOUND;
        }
        if (free_space_offset + record_size(key, data_size) > area_params[AREA_INDEX_WRITABLE].size) {
            return PDBSTORE_MEDIA_FULL;
        }
        offset = free_space_offset;
    } else {
        return ret;
    }

    // resilient flag set - write the record first to the staging area
    if (flags & PDBSTORE_RESILIENT_FLAG) {
        ret = pdbstore_write_record(AREA_INDEX_STAGING, key, data, data_size, flags, 0, &next_offset, FALSE);
        if (ret) {
            return ret;
        }
    }

    ret = pdbstore_write_record(AREA_INDEX_WRITABLE, key, data, data_size, flags, offset, &next_offset, replace_current);
    if (ret) {
        return ret;
    }

    if (!replace_current) {
        free_space_offset = next_offset;
    }

    if (flags & PDBSTORE_RESILIENT_FLAG) {
        ret = reset_area(AREA_INDEX_STAGING, 0);
        if (ret) {
            return ret;
        }
    }

    return PDBSTORE_SUCCESS;
}

// separate this for testing reasons
int pdbstore_init_readonly_area(int init_num_banks, pdbstore_bank_params_t *init_bank_params)
{
    uint16_t test = 0xabcd;
    big_endian = (((uint8_t *)&test)[0] == 0xab);

    memset(area_params, 0, sizeof(area_params));

    if (!init_num_banks || init_num_banks > PDBSTORE_MAX_BANKS) {
        return PDBSTORE_INVALID_ARGUMENT;
    }

    num_banks = init_num_banks;

    for (int i = 0; i < num_banks; i++) {
        memcpy(&bank_params[i], &init_bank_params[i], sizeof(pdbstore_bank_params_t));
    }

    area_params[AREA_INDEX_READONLY].bank_params = &bank_params[0];
    area_params[AREA_INDEX_READONLY].size = bank_params[0].bank_size;

    return PDBSTORE_SUCCESS;
}

int pdbstore_init(int init_num_banks, pdbstore_bank_params_t *init_bank_params)
{
    size_t actual_data_size, staging_data_size;
    int ret;
    master_record_data_t *master_rec;
    void *data_ptr, *staging_data_ptr;
    uint32_t offset, next_offset, writable_media_size;
    boolean staging_valid = FALSE, totally_corrupt;
    char key[PDBSTORE_MAX_KEY_SIZE];
    char staging_key[PDBSTORE_MAX_KEY_SIZE];
    uint8_t flags, staging_flags;

    if (initialized) {
        return PDBSTORE_SUCCESS;
    }
    ret = pdbstore_init_readonly_area(init_num_banks, init_bank_params);
    if (ret) {
        goto fail;
    }

    offset = 0;
    // Read readonly area first
    ret = read_record(AREA_INDEX_READONLY, offset, 0, &data_ptr, &actual_data_size,
                      FALSE, &totally_corrupt, &flags, &next_offset);
    master_rec = (master_record_data_t *) data_ptr;

    if (ret) {
        goto fail;
    }

    offset = next_offset;
    for (int i = 0; i < conv_ton16(master_rec->num_keys); i++) {
        ret = read_record(AREA_INDEX_READONLY, offset, 0, &data_ptr, &actual_data_size,
                          FALSE, &totally_corrupt, &flags, &next_offset);
        if (ret) {
            goto fail;
        }
        offset = next_offset;
    }

    area_params[AREA_INDEX_READONLY].size = next_offset;

    // Staging area should be able to contain largest record
    area_params[AREA_INDEX_STAGING].size = sizeof(record_header_t) + PDBSTORE_MAX_KEY_SIZE + PDBSTORE_MAX_DATA_SIZE;

    // Now calculate writable area address and size
    if (num_banks == 1) {
        writable_media_size = bank_params[0].bank_size - bank_params[0].start_offset;
        // Writable area should be able to contain at least one record
        if (writable_media_size < 2 * area_params[AREA_INDEX_STAGING].size + area_params[AREA_INDEX_READONLY].size) {
            return PDBSTORE_INVALID_ARGUMENT;
        }
        area_params[AREA_INDEX_WRITABLE].address = area_params[AREA_INDEX_READONLY].size;
        area_params[AREA_INDEX_WRITABLE].size = writable_media_size -
                                                (area_params[AREA_INDEX_STAGING].size + area_params[AREA_INDEX_READONLY].size);
        area_params[AREA_INDEX_WRITABLE].bank_params = &bank_params[0];
    } else if (num_banks == 2) {
        writable_media_size = bank_params[1].bank_size - bank_params[1].start_offset;
        if (writable_media_size < 2 * area_params[AREA_INDEX_STAGING].size) {
            return PDBSTORE_INVALID_ARGUMENT;
        }
        area_params[AREA_INDEX_WRITABLE].address = 0;
        area_params[AREA_INDEX_WRITABLE].size = writable_media_size - area_params[AREA_INDEX_STAGING].size;
        area_params[AREA_INDEX_WRITABLE].bank_params = &bank_params[1];
    } else {
        return PDBSTORE_INVALID_ARGUMENT;
    }

    area_params[AREA_INDEX_STAGING].address = area_params[AREA_INDEX_WRITABLE].address +
                                              area_params[AREA_INDEX_WRITABLE].size;
    area_params[AREA_INDEX_STAGING].bank_params = area_params[AREA_INDEX_WRITABLE].bank_params;

    // check if staging area is valid
    ret = read_record(AREA_INDEX_STAGING, 0, staging_key, &staging_data_ptr, &staging_data_size,
                      TRUE, &totally_corrupt, &staging_flags, &next_offset);
    if (ret == PDBSTORE_SUCCESS) {
        staging_valid = TRUE;
    } else if (ret != PDBSTORE_DATA_CORRUPT) {
        return ret;
    }


    // Now scan writable area to find free space offset and handle corrupt records
    offset = 0;
    while (offset < area_params[AREA_INDEX_WRITABLE].size) {
        ret = read_record(AREA_INDEX_WRITABLE, offset, key, &data_ptr, &actual_data_size,
                          TRUE, &totally_corrupt, &flags, &free_space_offset);
        // If staging area is valid and we reached the same key here, overwrite the record
        // with the one from staging area (providing it's not totally corrupt, meaning
        // that we can't move on, as the sizes are off the scale).
        if (((ret == PDBSTORE_SUCCESS) || (ret == PDBSTORE_DATA_CORRUPT)) &&
                staging_valid && !strcmp(key, staging_key) && !totally_corrupt) {

            ret = pdbstore_write_record(AREA_INDEX_WRITABLE, key, staging_data_ptr, staging_data_size,
                                        staging_flags, offset, &next_offset, TRUE);
            if (ret) {
                goto fail;
            }
            staging_valid = FALSE;
        }
        if (ret == PDBSTORE_DATA_CORRUPT) {
            ret = reset_area(AREA_INDEX_WRITABLE, offset);
            if (ret != PDBSTORE_SUCCESS) {
                goto fail;
            }
            free_space_offset = offset;
            break;
        }
        if (ret != PDBSTORE_SUCCESS) {
            goto fail;
        }
        offset = free_space_offset;
    }

    // If staging valid flag still set, this means that the we have a new record in staging area.
    // Write it to the end of our storage.
    if (staging_valid) {
        ret = pdbstore_write_record(AREA_INDEX_WRITABLE, staging_key, staging_data_ptr, staging_data_size,
                                    staging_flags, offset, &free_space_offset, FALSE);
        if (ret) {
            goto fail;
        }
    }

    // Clear staging area
    ret = reset_area(AREA_INDEX_STAGING, 0);
    if (ret != PDBSTORE_SUCCESS) {
        goto fail;
    }

    initialized = TRUE;
    return PDBSTORE_SUCCESS;

fail:
    // TODO: Assert?
    return ret;
}

int pdbstore_deinit()
{
    initialized = FALSE;
    return PDBSTORE_SUCCESS;
}

int pdbstore_get(const char *key, void **data, size_t *actual_size)
{
    int ret;
    uint32_t offset;
    area_index_e area;
    uint8_t flags;

    if (!is_valid_key(key)) {
        return PDBSTORE_INVALID_ARGUMENT;
    }

    if (!initialized) {
        return PDBSTORE_NOT_INITIALIZED;
    }

    ret = find_record(key, data, actual_size, &flags, &area, &offset);
    if (ret) {
        return ret;
    }

    if (flags & DELETE_FLAG) {
        return PDBSTORE_NOT_FOUND;
    }
    return PDBSTORE_SUCCESS;
}

int pdbstore_set(const char *key, const void *data, size_t data_size, uint8_t flags)
{
    if (flags & ~SUPPORTED_USER_FLAGS) {
        return PDBSTORE_INVALID_ARGUMENT;
    }
    return pdbstore_do_set(key, data, data_size, flags);
}

int pdbstore_remove(const char *key)
{
    return pdbstore_do_set(key, 0, 0, DELETE_FLAG);
}

int pdbstore_reset()
{
    if (!initialized) {
        return PDBSTORE_NOT_INITIALIZED;
    }

    int ret = reset_area(AREA_INDEX_WRITABLE, 0);
    if (ret) {
        return ret;
    }

    pdbstore_deinit();

    return pdbstore_init(num_banks, bank_params);
}
