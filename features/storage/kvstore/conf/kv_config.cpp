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

#include "kv_config.h"
#include "KVStore.h"
#include "kv_map.h"
#include "BlockDevice.h"
#include "FileSystem.h"
#include "FileSystemStore.h"
#include "SlicingBlockDevice.h"
#include "FATFileSystem.h"
#include "LittleFileSystem.h"
#include "TDBStore.h"
#include "mbed_error.h"

#if COMPONENT_FLASHIAP
#include "FlashIAPBlockDevice.h"
#endif

#if COMPONENT_QSPIF
#include "QSPIFBlockDevice.h"
#endif

#if COMPONENT_SPIF
#include "SPIFBlockDevice.h"
#endif

#if COMPONENT_DATAFLASH
#include "DataFlashBlockDevice.h"
#endif

#if COMPONENT_SD
#include "SDBlockDevice.h"
#endif


using namespace mbed;

typedef struct {
    KVStore *kvstroe_main_instance;
    KVStore *internal_store;
    KVStore *external_store;
    BlockDevice *internal_bd;
    BlockDevice *external_bd;
    FileSystem *external_fs;
} kvstore_config_t;

static SingletonPtr<PlatformMutex> mutex;
static bool is_kv_config_initialize = false;
static kvstore_config_t kvstore_config;

#define INTERNAL_BLOCKDEVICE_NAME FLASHIAP

#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

#define _GET_FILESYSTEM_concat(dev, ...) _get_filesystem_##dev(__VA_ARGS__)
#define GET_FILESYSTEM(dev, ...) _GET_FILESYSTEM_concat(dev, __VA_ARGS__)

#define _GET_BLOCKDEVICE_concat(dev, ...) _get_blockdevice_##dev(__VA_ARGS__)
#define GET_BLOCKDEVICE(dev, ...) _GET_BLOCKDEVICE_concat(dev, __VA_ARGS__)

FileSystem *_get_filesystem_FAT(BlockDevice *bd, const char *mount)
{
    static FATFileSystem sdcard(mount, bd);
    return &sdcard;

}

FileSystem *_get_filesystem_LITTLE(BlockDevice *bd, const char *mount)
{
    static LittleFileSystem flash(mount, bd);
    return &flash;
}

FileSystemStore *_get_fileSystemStore(FileSystem *fs)
{
    static FileSystemStore fss(fs);
    return &fss;
}

FileSystem *_get_filesystem_default(BlockDevice *bd, const char *mount)
{
#if COMPONENT_QSPIF || COMPONENT_SPIF || COMPONENT_DATAFLASH
    return _get_filesystem_LITTLE( bd, mount);
#elif COMPONENT_SD
    return _get_filesystem_FAT(bd, mount);
#else
    return NULL;
#endif
}

BlockDevice *_get_blockdevice_FLASHIAP(bd_addr_t start_address, bd_size_t size)
{
#if COMPONENT_FLASHIAP

    if (start_address == 0 && size == 0) {
        static FlashIAPBlockDevice bd(MBED_CONF_FLASHIAP_BLOCK_DEVICE_BASE_ADDRESS, MBED_CONF_FLASHIAP_BLOCK_DEVICE_SIZE);
        return &bd;
    }

    if (start_address != 0 && size == 0) {
        static FlashIAPBlockDevice bd(start_address);
        return &bd;
    }

    if (start_address != 0 && size != 0) {
        static FlashIAPBlockDevice bd(start_address, size);
        return &bd;
    }

    FlashIAP flash;
    int ret = flash.init();
    if (ret) {
        return NULL;
    }

    bd_addr_t end_of_flash = flash.get_flash_size() + flash.get_flash_start();
    start_address = end_of_flash - size;

    flash.deinit();

    static FlashIAPBlockDevice bd(start_address, size);
    return &bd;

#else
    return NULL;
#endif
}

BlockDevice *_get_blockdevice_SPIF(bd_addr_t start_address, bd_size_t size)
{
#if COMPONENT_SPIF
    static SPIFBlockDevice bd(
        MBED_CONF_SPIF_DRIVER_SPI_MOSI,
        MBED_CONF_SPIF_DRIVER_SPI_MISO,
        MBED_CONF_SPIF_DRIVER_SPI_CLK,
        MBED_CONF_SPIF_DRIVER_SPI_CS,
        MBED_CONF_SPIF_DRIVER_SPI_FREQ
    );

    if (start_address == 0 && size == 0 ) {
        return &bd;
    }

    static SlicingBlockDevice sbd(&bd, start_address, size == 0 ? 0 : start_address + size);
    return &sbd;

#else
    return NULL;
#endif
}

BlockDevice *_get_blockdevice_QSPIF(bd_addr_t start_address, bd_size_t size)
{
#if COMPONENT_QSPIF
    static QSPIFBlockDevice bd(
        QSPI_FLASH1_IO0,
        QSPI_FLASH1_IO1,
        QSPI_FLASH1_IO2,
        QSPI_FLASH1_IO3,
        QSPI_FLASH1_SCK,
        QSPI_FLASH1_CSN,
        QSPIF_POLARITY_MODE_0,
        MBED_CONF_QSPIF_QSPI_FREQ);
    );

    if (start_address == 0 && size == 0 ) {
    return &bd;
}

static SlicingBlockDevice sbd(&bd, start_address, size == 0 ? 0 : start_address + size);
return &sbd;

#else
    return NULL;
#endif
}

BlockDevice *_get_blockdevice_DATAFLASH(bd_addr_t start_address, bd_size_t size)
{
#if COMPONENT_DATAFLASH
    static DataFlashBlockDevice bd(
        MBED_CONF_DATAFLASH_SPI_MOSI,
        MBED_CONF_DATAFLASH_SPI_MISO,
        MBED_CONF_DATAFLASH_SPI_CLK,
        MBED_CONF_DATAFLASH_SPI_CS
    );

    if (start_address == 0 && size == 0 ) {
        return &bd;
    }

    static SlicingBlockDevice sbd(&bd, start_address, size == 0 ? 0 : start_address + size);
    return &sdb;

#else
    return NULL;
#endif
}

BlockDevice *_get_blockdevice_SD(bd_addr_t start_address, bd_size_t size)
{
#if COMPONENT_SD
    static SDBlockDevice bd(
        MBED_CONF_SD_SPI_MOSI,
        MBED_CONF_SD_SPI_MISO,
        MBED_CONF_SD_SPI_CLK,
        MBED_CONF_SD_SPI_CS
    );

    if (start_address == 0 && size == 0 ) {
        return &bd;
    }

    static SlicingBlockDevice sbd(&bd, start_address, size == 0 ? 0 : start_address + size);
    return &sbd;


#else
    return NULL;
#endif
}

BlockDevice *_get_blockdevice_default(bd_addr_t start_address, bd_size_t size)
{
#if COMPONENT_QSPIF
    return _get_blockdevice_QSPIF(start_address, size);
#elif COMPONENT_SPIF
    return _get_blockdevice_SPIF(start_address, size);
#elif COMPONENT_DATAFLASH
    return _get_blockdevice_DATAFLASH(start_address, size);
#elif COMPONENT_SD
    return _get_blockdevice_SD(start_address, size);
#else
    return NULL;
#endif
}

int _storage_config_TDB_INTERNAL()
{
    bd_size_t internal_size = MBED_CONF_STORAGE_TDB_INTERNAL_INTERNAL_SIZE;
    bd_addr_t internal_start_address = MBED_CONF_STORAGE_TDB_INTERNAL_INTERNAL_BASE_ADDRESS;

    kvstore_config.internal_bd = GET_BLOCKDEVICE(INTERNAL_BLOCKDEVICE_NAME, internal_start_address, internal_size);
    if (kvstore_config.internal_bd == NULL) {
        return MBED_ERROR_FAILED_OPERATION;
    }

    int ret = kvstore_config.internal_bd->init();
    if (ret != MBED_SUCCESS) {
        return MBED_ERROR_FAILED_OPERATION;
    }

    static TDBStore tdb_internal(kvstore_config.internal_bd);
    kvstore_config.internal_store = &tdb_internal;

    ret = kvstore_config.internal_store->init();
    if (ret != MBED_SUCCESS) {
        return ret;
    }

    kvstore_config.kvstroe_main_instance =
        kvstore_config.internal_store; //TODO: change this when secure storage come to live

    ret = kv_init();
    if (MBED_SUCCESS != ret) {
        return ret;
    }

    ret = kv_attach(STR(MBED_CONF_STORAGE_DEFAULT_KV), kvstore_config.kvstroe_main_instance);
    if (MBED_SUCCESS != ret) {
        return ret;
    }

    return MBED_SUCCESS;
}

int _storage_config_TDB_EXTERNAL()
{
    bd_size_t internal_rbp_size = MBED_CONF_STORAGE_TDB_EXTERNAL_RBP_INTERNAL_SIZE;
    size_t rbp_num_of_enteries = MBED_CONF_STORAGE_TDB_EXTERNAL_RBP_NUMBER_OF_ENTRIES;
    bd_addr_t internal_start_address = MBED_CONF_STORAGE_TDB_EXTERNAL_INTERNAL_BASE_ADDRESS;

    if (internal_rbp_size == 0) {
        internal_rbp_size = 4 * 1024 * rbp_num_of_enteries / 32;
    }

    kvstore_config.internal_bd = GET_BLOCKDEVICE(INTERNAL_BLOCKDEVICE_NAME, internal_start_address, internal_rbp_size);
    if (kvstore_config.internal_bd == NULL) {
        return MBED_ERROR_FAILED_OPERATION ;
    }

    int ret = kvstore_config.internal_bd->init();
    if (ret != MBED_SUCCESS) {
        return MBED_ERROR_FAILED_OPERATION ;
    }

    static TDBStore tdb_internal(kvstore_config.internal_bd);
    kvstore_config.internal_store = &tdb_internal;

    ret = kvstore_config.internal_store->init();
    if (ret != MBED_SUCCESS) {
        return ret;
    }

    return _storage_config_TDB_EXTERNAL_NO_RBP();
}

int _storage_config_TDB_EXTERNAL_NO_RBP()
{
    bd_size_t size = MBED_CONF_STORAGE_TDB_EXTERNAL_EXTERNAL_SIZE;
    bd_addr_t address = MBED_CONF_STORAGE_TDB_EXTERNAL_EXTERNAL_BASE_ADDRESS;

    kvstore_config.external_bd = GET_BLOCKDEVICE(MBED_CONF_STORAGE_TDB_EXTERNAL_BLOCKDEVICE, address, size);
    if (kvstore_config.external_bd == NULL) {
        return MBED_ERROR_FAILED_OPERATION ;
    }

    int ret = kvstore_config.external_bd->init();
    if (ret != MBED_SUCCESS) {
        return MBED_ERROR_FAILED_OPERATION ;
    }

    static TDBStore tdb_external(kvstore_config.external_bd);
    kvstore_config.external_store = &tdb_external;

    kvstore_config.kvstroe_main_instance =
        kvstore_config.external_store; //TODO: change this when secure storage come to live

    ret = kvstore_config.external_store->init();
    if (ret != MBED_SUCCESS) {
        return ret;
    }

    ret = kv_init();
    if (MBED_SUCCESS != ret) {
        return ret;
    }

    ret = kv_attach(STR(MBED_CONF_STORAGE_DEFAULT_KV), kvstore_config.kvstroe_main_instance);
    if (MBED_SUCCESS != ret) {
        return ret;
    }

    return MBED_SUCCESS;
}

int _storage_config_FILESYSTEM()
{
    bd_size_t internal_rbp_size = MBED_CONF_STORAGE_FILESYSTEM_RBP_INTERNAL_SIZE;
    size_t rbp_num_of_enteries = MBED_CONF_STORAGE_FILESYSTEM_RBP_NUMBER_OF_ENTRIES;
    bd_addr_t internal_start_address = MBED_CONF_STORAGE_FILESYSTEM_INTERNAL_BASE_ADDRESS;

    if (internal_rbp_size == 0) {
        internal_rbp_size = 4 * 1024 * rbp_num_of_enteries / 32;
    }

    kvstore_config.internal_bd = GET_BLOCKDEVICE(INTERNAL_BLOCKDEVICE_NAME, internal_start_address, internal_rbp_size);
    if (kvstore_config.internal_bd == NULL) {
        return MBED_ERROR_FAILED_OPERATION ;
    }

    int ret = kvstore_config.internal_bd->init();
    if (ret != MBED_SUCCESS) {
        return MBED_ERROR_FAILED_OPERATION ;
    }

    static TDBStore tdb_internal(kvstore_config.internal_bd);
    kvstore_config.internal_store = &tdb_internal;

    ret = kvstore_config.internal_store->init();
    if (ret != MBED_SUCCESS) {
        return ret;
    }

    return _storage_config_FILESYSTEM_NO_RBP();
}

int _storage_config_FILESYSTEM_NO_RBP()
{
    bd_size_t size = MBED_CONF_STORAGE_FILESYSTEM_RBP_INTERNAL_SIZE;
    bd_addr_t address = MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_EXTERNAL_BASE_ADDRESS;
    const char *mount_point = STR(MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_MOUNT_POINT);

    kvstore_config.external_bd = GET_BLOCKDEVICE(MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_BLOCKDEVICE, address, size);
    if (kvstore_config.external_bd == NULL) {
        return MBED_ERROR_FAILED_OPERATION ;
    }

    int ret = kvstore_config.external_bd->init();
    if (MBED_SUCCESS != ret) {
        return MBED_ERROR_FAILED_OPERATION ;
    }

    kvstore_config.external_fs = GET_FILESYSTEM(MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_FILESYSTEM, kvstore_config.external_bd,
                                 mount_point);
    if (kvstore_config.external_bd == NULL) {
        return MBED_ERROR_FAILED_OPERATION ;
    }

    ret = kvstore_config.external_fs->mount(kvstore_config.external_bd);
    if (kvstore_config.external_bd == NULL) {
        return MBED_ERROR_FAILED_OPERATION ;
    }

    kvstore_config.external_store = _get_fileSystemStore(kvstore_config.external_fs);
    if (kvstore_config.external_bd == NULL) {
        return MBED_ERROR_FAILED_OPERATION ;
    }

    ret = kvstore_config.external_store->init();
    if (ret != MBED_SUCCESS) {
        return ret;
    }

    kvstore_config.kvstroe_main_instance =
        kvstore_config.external_store; //TODO: change this when secure storage come to live

    ret = kv_init();
    if (MBED_SUCCESS != ret) {
        return ret;
    }

    ret = kv_attach(STR(MBED_CONF_STORAGE_DEFAULT_KV), kvstore_config.kvstroe_main_instance);
    if (MBED_SUCCESS != ret) {
        return ret;
    }

    return MBED_SUCCESS;
}

int storage_configuration()
{

    int ret = MBED_SUCCESS;

    mutex->lock();

    if (is_kv_config_initialize) {
        goto exit;
    }

    memset(&kvstore_config, 0, sizeof(kvstore_config_t));

    ret = _STORAGE_CONFIG(MBED_CONF_STORAGE_STORAGE_TYPE);

    if (ret == MBED_SUCCESS) {
        is_kv_config_initialize = true;
    }

exit:
    mutex->unlock();
    return ret;
}
