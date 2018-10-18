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
#ifndef _KV_CONFIG
#define _KV_CONFIG

#ifdef __cplusplus
extern "C" {
#endif

#if MBED_CONF_STORAGE_STORAGE_TYPE == FILESYSTEM
#define FSST_FOLDER_PATH MBED_CONF_STORAGE_FILESYSTEM_FOLDER_PATH
#elif MBED_CONF_STORAGE_STORAGE_TYPE == FILESYSTEM_NO_RBP
#define FSST_FOLDER_PATH MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_FOLDER_PATH
#endif

#if MBED_CONF_STORAGE_STORAGE == NULL
#define MBED_CONF_STORAGE_STORAGE USER_DEFINED
#endif

#define _STORAGE_CONFIG_concat(dev) _storage_config_##dev()
#define _STORAGE_CONFIG(dev) _STORAGE_CONFIG_concat(dev)

/**
 * @brief This function initializes internal memory secure storage
 *        This includes a TDBStore instance with a FlashIAPBlockdevice
 *        as the supported storage.
 *        The following is a list of configuration parameter
 *        MBED_CONF_STORAGE_TDB_INTERNAL_SIZE - The size of the underlying FlashIAPBlockdevice
 *        MBED_CONF_STORAGE_TDB_INTERNAL_BASE_ADDRESS - The start address of the underlying FlashIAPBlockdevice
 * @returns 0 on success or negative value on failure.
 */
int _storage_config_TDB_INTERNAL();

/**
 * @brief This function initialize external memory secure storage
 *        This includes a SecureStore class with TDBStore over FlashIAPBlockdevice
 *        and an external TDBStore over a default blockdevice unless configured differently.
 *        The following is a list of configuration parameter:
 *        MBED_CONF_STORAGE_TDB_EXTERNAL_RBP_INTERNAL_SIZE - Size of the internal FlashIAPBlockDevice and by
 *        default is set to 4K*#enteries/32. The start address will be set to end of flash - rbp_internal_size.
 *        MBED_CONF_STORAGE_TDB_EXTERNAL_RBP_NUMBER_OF_ENTRIES - If not defined default is 64
 *        MBED_CONF_STORAGE_TDB_EXTERNAL_INTERNAL_BASE_ADDRESS - The satrt address of the internal FlashIAPBlockDevice.
 *        MBED_CONF_STORAGE_TDB_EXTERNAL_EXTERNAL_SIZE - Size of the external blockdevice in bytes or NULL for
 *        max possible size.
 *        MBED_CONF_STORAGE_TDB_EXTERNAL_EXTERNAL_BASE_ADDRESS - The block device start address.
 *        MBED_CONF_STORAGE_TDB_EXTERNAL_EXTERNAL_BLOCK_DEVICE - Alowed vlaues are: default, SPIF, DATAFASH, QSPIF or SD
 * @returns 0 on success or negative value on failure.
 */
int _storage_config_TDB_EXTERNAL();

/**
 * @brief This function initialize a external memory secure storage
 *        This includes a SecureStore class with external TDBStore over a blockdevice or,
 *        if no blockdevice was set the default blockdevice will be used.
 *        The following is a list of configuration parameter:
 *        MBED_CONF_STORAGE_TDB_EXTERNAL_NO_RBP_EXTERNAL_SIZE - Size of the external blockdevice in bytes
 *                                                              or NULL for max possible size.
 *        MBED_CONF_STORAGE_TDB_EXTERNAL_NO_RBP_EXTERNAL_BASE_ADDRESS - The block device start address
 *        MBED_CONF_STORAGE_TDB_EXTERNAL_NO_RBP_EXTERNAL_BLOCK_DEVICE - Alowed vlaues are: default, SPIF, DATAFASH, QSPIF or SD
 * @returns 0 on success or negative value on failure.
 */
int _storage_config_TDB_EXTERNAL_NO_RBP();

/**
 * @brief This function initialize a FILESYSTEM memory secure storage
 *        This includes a SecureStore class with TDBStore over FlashIAPBlockdevice
 *        in the internal memory and an external FileSysteStore. If blockdevice and filesystem not set,
 *        the system will use the default block device and default filesystem
 *        The following is a list of configuration parameter:
 *        MBED_CONF_STORAGE_FILESYSTEM_RBP_INTERNAL_SIZE - Size of the internal FlashIAPBlockDevice and by default is
 *                                                         set to 4K*#enteries/32. The start address will be set to
 *                                                         end of flash - rbp_internal_size.
 *        MBED_CONF_STORAGE_FILESYSTEM_RBP_NUMBER_OF_ENTRIES - If not defined default is 64
 *        MBED_CONF_STORAGE_FILESYSTEM_INTERNAL_BASE_ADDRESS - The satrt address of the internal FlashIAPBlockDevice.
 *        MBED_CONF_STORAGE_FILESYSTEM_FILESYSTEM - Allowed values are: default, FAT or LITTLE
 *        MBED_CONF_STORAGE_FILESYSTEM_BLOCKDEVICE - Allowed values are: default, SPIF, DATAFASH, QSPIF or SD
 *        MBED_CONF_STORAGE_FILESYSTEM_EXTERNAL_SIZE - External Blockdevice size in bytes or NULL for max possible size.
 *        MBED_CONF_STORAGE_FILESYSTEM_EXTERNAL_BASE_ADDRESS - The block device start address.
 *        MBED_CONF_STORAGE_FILESYSTEM_MOUNT_POINT - Where to mount the filesystem
 *        MBED_CONF_STORAGE_FILESYSTEM_FOLDER_PATH - The working folder paths
 *
 * @returns 0 on success or negative value on failure.
 */
int _storage_config_FILESYSTEM();

/**
 * @brief This function initialize a FILESYSTEM_NO_RBP memory secure storage with no
 *        rollback protection. This includes a SecureStore class an external FileSysteStore over a default
 *        filesystem with default blockdevice unless differently configured.
 *        The following is a list of configuration parameter:
 *        MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_FILESYSTEM - Allowed values are: default, FAT or LITTLE
 *        MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_BLOCKDEVICE - Allowed values are: default, SPIF, DATAFASH, QSPIF or SD
 *        MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_EXTERNAL_SIZE - Blockdevice size in bytes. or NULL for max possible size.
 *        MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_EXTERNAL_BASE_ADDRESS - The block device start address.
 *        MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_MOUNT_POINT - Where to mount the filesystem
 *        MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_FOLDER_PATH - The working folder paths
 *
 * @returns 0 on success or negative value on failure.
 */
int _storage_config_FILESYSTEM_NO_RBP();

/**
 * @brief This function will initialize one of the configuration exists in mbed-os. In order to overwite
 *        the default configuration please overwrite this function.
 *
 * @returns 0 on success or negative value on failure.
 */
int storage_configuration();

#ifdef __cplusplus
} // closing brace for extern "C"
#endif
#endif
