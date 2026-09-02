/* Copyright 2020-2024 Espressif Systems (Shanghai) CO LTD
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/param.h>
#include <assert.h>
#include "esp_loader_io.h"
#include "esp_loader.h"
#include "app_common.h"

#ifndef SINGLE_TARGET_SUPPORT


// For esp32, esp32s2
#define BOOTLOADER_ADDRESS_V0       0x1000
// For esp8266, esp32s3 and later chips
#define BOOTLOADER_ADDRESS_V1       0x0
// For esp32c5 and esp32p4
#define BOOTLOADER_ADDRESS_V2       0x2000
#define PARTITION_ADDRESS           0x8000
#define APPLICATION_ADDRESS         0x10000


extern const uint8_t  ESP32_C6_bootloader_bin[];
extern const uint32_t ESP32_C6_bootloader_bin_size;
extern const uint8_t  ESP32_C6_bootloader_bin_md5[];
extern const uint8_t  ESP32_C6_hello_world_bin[];
extern const uint32_t ESP32_C6_hello_world_bin_size;
extern const uint8_t  ESP32_C6_hello_world_bin_md5[];
extern const uint8_t  ESP32_C6_partition_table_bin[];
extern const uint32_t ESP32_C6_partition_table_bin_size;
extern const uint8_t  ESP32_C6_partition_table_bin_md5[];

extern const uint8_t  ESP32_C6_app_bin[];
extern const uint32_t ESP32_C6_app_bin_size;

void get_example_binaries(target_chip_t target, example_binaries_t *bins)
{
    if (target == ESP32C6_CHIP) {
        bins->boot.data = ESP32_C6_bootloader_bin;
        bins->boot.size = ESP32_C6_bootloader_bin_size;
        bins->boot.md5 = ESP32_C6_bootloader_bin_md5;
        bins->boot.addr = BOOTLOADER_ADDRESS_V1;
        bins->part.data = ESP32_C6_partition_table_bin;
        bins->part.size = ESP32_C6_partition_table_bin_size;
        bins->part.md5 = ESP32_C6_partition_table_bin_md5;
        bins->part.addr = PARTITION_ADDRESS;
        bins->app.data  = ESP32_C6_hello_world_bin;
        bins->app.size  = ESP32_C6_hello_world_bin_size;
        bins->app.md5 = ESP32_C6_hello_world_bin_md5;
        bins->app.addr  = APPLICATION_ADDRESS;
    } else {
        abort();
    }
}

void get_example_ram_app_binary(target_chip_t target, example_ram_app_binary_t *bin)
{
    switch (target) {
    	case ESP32C6_CHIP:
    		bin->ram_app.data = ESP32_C6_app_bin;
    		bin->ram_app.size = ESP32_C6_app_bin_size;
    		break;

    	default:
    		abort();
    }
}

#endif

const char *get_error_string(const esp_loader_error_t error)
{
    const char *mapping[ESP_LOADER_ERROR_INVALID_RESPONSE + 1] = {
        "NONE", "UNKNOWN", "TIMEOUT", "IMAGE SIZE",
        "INVALID MD5", "INVALID PARAMETER", "INVALID TARGET",
        "UNSUPPORTED CHIP", "UNSUPPORTED FUNCTION", "INVALID RESPONSE"
    };

    assert(error <= ESP_LOADER_ERROR_INVALID_RESPONSE);

    return mapping[error];
}

esp_loader_error_t connect_to_target(uint32_t higher_transmission_rate)
{
    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();

    esp_loader_error_t err = esp_loader_connect(&connect_config);
    if (err != ESP_LOADER_SUCCESS) {
        printf("Cannot connect to target. Error: %s\r\n", get_error_string(err));

        if (err == ESP_LOADER_ERROR_TIMEOUT) {
            printf("Check if the host and the target are properly connected.\r\n");
        } else if (err == ESP_LOADER_ERROR_INVALID_TARGET) {
            printf("You could be using an unsupported chip, or chip revision.\r\n");
        } else if (err == ESP_LOADER_ERROR_INVALID_RESPONSE) {
            printf("Try lowering the transmission rate or using shorter wires to connect the host and the target.\r\n");
        }

        return err;
    }
    printf("Connected to target\r\n");

#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)
    if (higher_transmission_rate && esp_loader_get_target() != ESP8266_CHIP) {
        err = esp_loader_change_transmission_rate(higher_transmission_rate);
        if (err == ESP_LOADER_ERROR_UNSUPPORTED_FUNC) {
            printf("ESP8266 does not support change transmission rate command.");
            return err;
        } else if (err != ESP_LOADER_SUCCESS) {
            printf("Unable to change transmission rate on target.");
            return err;
        } else {
            err = loader_port_change_transmission_rate(higher_transmission_rate);
            if (err != ESP_LOADER_SUCCESS) {
                printf("Unable to change transmission rate.");
                return err;
            }
            printf("Transmission rate changed.\r\n");
        }
    }
#endif /* SERIAL_FLASHER_INTERFACE_UART || SERIAL_FLASHER_INTERFACE_USB */

    return ESP_LOADER_SUCCESS;
}

#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)
esp_loader_error_t connect_to_target_with_stub(const uint32_t current_transmission_rate,
        const uint32_t higher_transmission_rate)
{
    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();

    esp_loader_error_t err = esp_loader_connect_with_stub(&connect_config);
    if (err != ESP_LOADER_SUCCESS) {
        printf("Cannot connect to target. Error: %s\r\n", get_error_string(err));

        if (err == ESP_LOADER_ERROR_TIMEOUT) {
            printf("Check if the host and the target are properly connected.\r\n");
        } else if (err == ESP_LOADER_ERROR_INVALID_TARGET) {
            printf("You could be using an unsupported chip, or chip revision.\r\n");
        } else if (err == ESP_LOADER_ERROR_INVALID_RESPONSE) {
            printf("Try lowering the transmission rate or using shorter wires to connect the host and the target.\r\n");
        }

        return err;
    }
    printf("Connected to target\r\n");

    if (higher_transmission_rate != current_transmission_rate) {
        err = esp_loader_change_transmission_rate_stub(current_transmission_rate,
                higher_transmission_rate);

        if (err == ESP_LOADER_ERROR_UNSUPPORTED_FUNC) {
            printf("ESP8266 does not support change transmission rate command.");
            return err;
        } else if (err != ESP_LOADER_SUCCESS) {
            printf("Unable to change transmission rate on target.");
            return err;
        } else {
            err = loader_port_change_transmission_rate(higher_transmission_rate);
            if (err != ESP_LOADER_SUCCESS) {
                printf("Unable to change transmission rate.");
                return err;
            }
            printf("Transmission rate changed.\r\n");
        }
    }

    return ESP_LOADER_SUCCESS;
}
#endif /* SERIAL_FLASHER_INTERFACE_UART || SERIAL_FLASHER_INTERFACE_USB */

#ifndef SERIAL_FLASHER_INTERFACE_SPI
esp_loader_error_t flash_binary(const uint8_t *bin, size_t size, size_t address)
{
    esp_loader_error_t err;
    static uint8_t payload[1024];
    const uint8_t *bin_addr = bin;

    printf("Erasing flash (this may take a while)...\r\n");
    err = esp_loader_flash_start(address, size, sizeof(payload));
    if (err != ESP_LOADER_SUCCESS) {
        printf("Erasing flash failed with error: %s.\r\n", get_error_string(err));

        if (err == ESP_LOADER_ERROR_INVALID_PARAM) {
            printf("If using Secure Download Mode, double check that the specified "
                   "target flash size is correct.\r\n");
        }
        return err;
    }
    printf("Start programming\r\n");

    size_t binary_size = size;
    size_t written = 0;

    while (size > 0) {
        size_t to_read = MIN(size, sizeof(payload));
        memcpy(payload, bin_addr, to_read);

        err = esp_loader_flash_write(payload, to_read);
        if (err != ESP_LOADER_SUCCESS) {
            printf("\nPacket could not be written! Error %s.\r\n", get_error_string(err));
            return err;
        }

        size -= to_read;
        bin_addr += to_read;
        written += to_read;

        int progress = (int)(((float)written / binary_size) * 100);
        printf("\rProgress: %d %%", progress);
    };

    printf("\nFinished programming\r\n");

#ifdef MD5_ENABLED
    err = esp_loader_flash_verify();
    if (err == ESP_LOADER_ERROR_UNSUPPORTED_FUNC) {
        printf("ESP8266 does not support flash verify command.");
        return err;
    } else if (err != ESP_LOADER_SUCCESS) {
        printf("MD5 does not match. Error: %s\r\n", get_error_string(err));
        return err;
    }
    printf("Flash verified\r\n");
#endif

    return ESP_LOADER_SUCCESS;
}
#endif /* SERIAL_FLASHER_INTERFACE_SPI */

esp_loader_error_t load_ram_binary(const uint8_t *bin)
{
    printf("Start loading\r\n");
    esp_loader_error_t err;
    const esp_loader_bin_header_t *header = (const esp_loader_bin_header_t *)bin;
    esp_loader_bin_segment_t segments[header->segments];

    // Parse segments
    uint32_t seg;
    uint32_t *cur_seg_pos;
    // ESP8266 does not have extended header
    uint32_t offset = esp_loader_get_target() == ESP8266_CHIP ? BIN_HEADER_SIZE : BIN_HEADER_EXT_SIZE;
    for (seg = 0, cur_seg_pos = (uint32_t *)(&bin[offset]);
            seg < header->segments;
            seg++) {
        segments[seg].addr = *cur_seg_pos++;
        segments[seg].size = *cur_seg_pos++;
        segments[seg].data = (uint8_t *)cur_seg_pos;
        cur_seg_pos += (segments[seg].size) / 4;
    }

    // Download segments
    for (seg = 0; seg < header->segments; seg++) {
        printf("Downloading %"PRIu32" bytes at 0x%08"PRIx32"...\r\n", segments[seg].size, segments[seg].addr);

        err = esp_loader_mem_start(segments[seg].addr, segments[seg].size, ESP_RAM_BLOCK);
        if (err != ESP_LOADER_SUCCESS) {
            printf("Loading to RAM could not be started. Error: %s.\r\n", get_error_string(err));

            if (err == ESP_LOADER_ERROR_INVALID_PARAM) {
                printf("Check if the chip has Secure Download Mode enabled.\r\n");
            }
            return err;
        }

        size_t remain_size = segments[seg].size;
        const uint8_t *data_pos = segments[seg].data;
        while (remain_size > 0) {
            size_t data_size = MIN(ESP_RAM_BLOCK, remain_size);
            err = esp_loader_mem_write(data_pos, data_size);
            if (err != ESP_LOADER_SUCCESS) {
                printf("\nPacket could not be written! Error: %s.\r\n", get_error_string(err));
                return err;
            }
            data_pos += data_size;
            remain_size -= data_size;
        }
    }

    err = esp_loader_mem_finish(header->entrypoint);
    if (err != ESP_LOADER_SUCCESS) {
        printf("\nLoading to RAM finished with error: %s.\r\n", get_error_string(err));
        return err;
    }
    printf("\nFinished loading\r\n");

    return ESP_LOADER_SUCCESS;
}
