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
#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"

#include "EncryptedBlockDevice.h"
#include <stdlib.h>
#include "HeapBlockDevice.h"
#include "FATFileSystem.h"
#include "LittleFileSystem.h"
#include "File.h"
#include "Timer.h"
#include "nvstore.h"

using namespace utest::v1;

void rw_test_fill(EncryptedBlockDevice &enc_bd, int seed, uint8_t *buf, int erase,
                  bd_addr_t addr, bd_size_t size)
{
    int err;

    srand(seed);
    for (bd_size_t i = 0; i < size; i++) {
        buf[i] = 0xff & rand();
    }

    if (erase) {
        bd_size_t erase_size = enc_bd.get_erase_size();
        for (bd_addr_t erase_addr = addr / erase_size * erase_size; erase_addr < addr + size;
             erase_addr += erase_size) {
            err = enc_bd.erase(erase_addr, erase_size);
            TEST_ASSERT_EQUAL(0, err);
        }
    }

    err = enc_bd.program(buf, addr, size);
    TEST_ASSERT_EQUAL(0, err);
}

void rw_test_read_check(EncryptedBlockDevice &enc_bd, int seed, uint8_t *buf,
                        bd_addr_t addr, bd_size_t size)
{
    int err;

    err = enc_bd.read(buf, addr, size);
    TEST_ASSERT_EQUAL(0, err);

    srand(seed);
    for (bd_size_t i = 0; i < size; i++) {
        TEST_ASSERT_EQUAL(0xff & rand(), buf[i]);
    }
}

// Read/write test directly over block device
void test_direct_read_write()
{
    NVStore &nvstore = NVStore::get_instance();
    nvstore.set_max_keys(16); // Simulate an existing system
    nvstore.reset();

    HeapBlockDevice bd(128*1024, 4, 4, 1024);
    EncryptedBlockDevice enc_bd(&bd);
    int err = enc_bd.init();
    TEST_ASSERT_EQUAL(0, err);

    bd_size_t erase_size = bd.get_erase_size();

    uint8_t *write_buf = new uint8_t[3*erase_size];
    uint8_t *read_buf = new uint8_t[3*erase_size];

    TEST_ASSERT(write_buf);
    TEST_ASSERT(read_buf);

    rw_test_fill(enc_bd, 1, write_buf, 1, 4, erase_size+12);
    rw_test_fill(enc_bd, 2, write_buf, 1, 2*erase_size+16, erase_size-12);
    rw_test_fill(enc_bd, 3, write_buf, 1, 5*erase_size-8, 2*erase_size-4);
    rw_test_fill(enc_bd, 4, write_buf, 1, 9*erase_size, 32);
    rw_test_fill(enc_bd, 5, write_buf, 1, 10*erase_size+8, 4);

    rw_test_read_check(enc_bd, 4, read_buf, 9*erase_size, 32);
    rw_test_read_check(enc_bd, 5, read_buf, 10*erase_size+8, 4);
    rw_test_read_check(enc_bd, 1, read_buf, 4, erase_size+12);
    rw_test_read_check(enc_bd, 2, read_buf, 2*erase_size+16, erase_size-12);
    rw_test_read_check(enc_bd, 3, read_buf, 5*erase_size-8, 2*erase_size-4);

    rw_test_fill(enc_bd, 6, write_buf, 1, 2*erase_size+24, erase_size+48);
    rw_test_fill(enc_bd, 7, write_buf, 1, 6*erase_size, erase_size+12);
    rw_test_fill(enc_bd, 8, write_buf, 1, 5*erase_size+96, erase_size-96);
    rw_test_fill(enc_bd, 9, write_buf, 0, 7*erase_size+100, 28);

    rw_test_read_check(enc_bd, 6, read_buf, 2*erase_size+24, erase_size+48);
    rw_test_read_check(enc_bd, 8, read_buf, 5*erase_size+96, erase_size-96);
    rw_test_read_check(enc_bd, 7, read_buf, 6*erase_size, erase_size+12);
    rw_test_read_check(enc_bd, 9, read_buf, 7*erase_size+100, 28);

    rw_test_fill(enc_bd, 10, write_buf, 1, 5*erase_size+200, 3*erase_size);
    rw_test_read_check(enc_bd, 10, read_buf, 5*erase_size+200, 3*erase_size);

    // Intentional authentication failure test
    rw_test_fill(enc_bd, 11, write_buf, 1, 15*erase_size+128, erase_size);
    err = bd.read(read_buf, 16*erase_size, 4);
    TEST_ASSERT_EQUAL(0, err);
    read_buf[0]++;
    err = bd.program(read_buf, 16*erase_size, 4);
    TEST_ASSERT_EQUAL(0, err);
    err = enc_bd.read(read_buf, 15*erase_size+128, erase_size);
    TEST_ASSERT_EQUAL(BD_ERROR_ENC_BD_AUTHENTICATION_FAILED, err);

    err = enc_bd.deinit();
    TEST_ASSERT_EQUAL(0, err);

    delete[] write_buf;
    delete[] read_buf;
}

// Read/write test over file system on top of block device - both correctness and performance
template <class FILE_SYSTEM>
void test_fs_read_write(BlockDevice *bd)
{
    NVStore &nvstore = NVStore::get_instance();
    FILE_SYSTEM fs("fs");
    nvstore.reset();
    // This test also measures performance (comparing to underlying BD)
    Timer timer;
    const int file_size = 32;
    const int num_iterations = 10;

    int err = bd->init();
    TEST_ASSERT_EQUAL(0, err);

    uint8_t *buf = new uint8_t[file_size];

    int elapsed;

    timer.start();

    err = FILE_SYSTEM::format(bd);
    TEST_ASSERT_EQUAL(0, err);
    elapsed = timer.read_ms();
    printf("Elapsed time for format is %d millseconds\n", elapsed);

    timer.reset();

    err = fs.mount(bd);
    TEST_ASSERT_EQUAL(0, err);

    elapsed = timer.read_ms();
    printf("Elapsed time for mount is %d millseconds\n", elapsed);

    for (int i = 0; i < num_iterations; i++) {
        srand(1);
        for (bd_size_t j = 0; j < file_size; j++) {
            buf[j] = 0xff & rand();
        }

        printf ("\nIteration %d\n", i);
        timer.reset();

        File file;
        err = file.open(&fs, "file1.dat", O_WRONLY | O_CREAT);
        TEST_ASSERT_EQUAL(0, err);
        ssize_t size = file.write(buf, file_size);
        TEST_ASSERT_EQUAL(file_size, size);
        err = file.close();
        TEST_ASSERT_EQUAL(0, err);

        elapsed = timer.read_ms();
        printf("Elapsed time for file write (%d bytes) is %d millseconds\n", file_size, elapsed);

        timer.reset();

        err = file.open(&fs, "file1.dat", O_RDONLY);
        TEST_ASSERT_EQUAL(0, err);
        size = file.read(buf, file_size);
        TEST_ASSERT_EQUAL(file_size, size);
        err = file.close();
        TEST_ASSERT_EQUAL(0, err);

        elapsed = timer.read_ms();
        printf("Elapsed time for file read (%d bytes) is %d millseconds\n", file_size, elapsed);

        srand(1);
        for (bd_size_t j = 0; j < file_size; j++) {
            TEST_ASSERT_EQUAL(0xff & rand(), buf[j]);
        }
    }

    timer.stop();

    err = fs.unmount();
    TEST_ASSERT_EQUAL(0, err);

    err = bd->deinit();
    TEST_ASSERT_EQUAL(0, err);

    delete[] buf;
}

void test_fs_read_write_fat()
{
    HeapBlockDevice bd(200*512, 4, 512, 512);
    EncryptedBlockDevice enc_bd(&bd);
    printf("\nStarting FAT over encrypted BD test\n");
    test_fs_read_write <FATFileSystem> (&enc_bd);
    printf("\nStarting FAT over non-encrypted BD test\n");
    test_fs_read_write <FATFileSystem> (&bd);
}

void test_fs_read_write_littlefs()
{
    HeapBlockDevice bd(128*4096, 4, 1, 4096);
    EncryptedBlockDevice enc_bd(&bd);
    printf("\nStarting LittleFS over encrypted BD test\n");
    test_fs_read_write <LittleFileSystem> (&enc_bd);
    printf("\nStarting LittleFS over non-encrypted BD test\n");
    test_fs_read_write <LittleFileSystem> (&bd);
}

// Test setup
utest::v1::status_t test_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(30, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

Case cases[] = {
        Case("Testing read/write directly over encrypted BD", test_direct_read_write),
        Case("Testing read/write over FAT on top of encrypted BD", test_fs_read_write_fat),
        Case("Testing read/write over LittleFS on top of encrypted BD", test_fs_read_write_littlefs)
};

Specification specification(test_setup, cases);

int main() {
    return !Harness::run(specification);
}
