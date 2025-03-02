/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <string_view>

#include "display.hpp"
#include "pico/bootrom.h"
#include "hardware/structs/rosc.h"
#include "hardware/watchdog.h"
#include "pico/timeout_helper.h"
#include "zlib.h"
#include "tcp_server/tcp_server.hpp"
#include "config_storage/config_storage.hpp"

#include "bsp/board.h"
#include "tusb.h"

#include "cdc_uart.h"
#include "get_serial.h"

// UART0 for Picoprobe debug
// UART1 for picoprobe to target device

using namespace pimoroni;

const size_t COMMAND_LEN = 4;
const size_t CONFIG_KEY_LEN = 16;
const size_t CONFIG_VALUE_LEN = 128;
uint8_t command_buffer[COMMAND_LEN];
uint8_t config_key_buffer[CONFIG_KEY_LEN];
uint8_t config_value_buffer[CONFIG_VALUE_LEN];

std::string_view command((const char *)command_buffer, COMMAND_LEN);


bool cdc_wait_for(std::string_view data, uint timeout_ms=1000) {
    timeout_state ts;
    absolute_time_t until = delayed_by_ms(get_absolute_time(), timeout_ms);
    check_timeout_fn check_timeout = init_single_timeout_until(&ts, until);

    for(auto expected_char : data) {
        char got_char;
        while(1){
            tud_task();
            if (cdc_task((uint8_t *)&got_char, 1) == 1) break;
            if(check_timeout(&ts, until)) return false;
        }
        if (got_char != expected_char) return false;
    }
    return true;
}

size_t cdc_get_bytes(const uint8_t *buffer, const size_t len, const uint timeout_ms=1000) {
    memset((void *)buffer, 0, len);

    uint8_t *p = (uint8_t *)buffer;

    timeout_state ts;
    absolute_time_t until = delayed_by_ms(get_absolute_time(), timeout_ms);
    check_timeout_fn check_timeout = init_single_timeout_until(&ts, until);

    size_t bytes_remaining = len;
    while (bytes_remaining && !check_timeout(&ts, until)) {
        tud_task(); // tinyusb device task
        size_t bytes_read = cdc_task(p, std::min(bytes_remaining, MAX_UART_PACKET));
        bytes_remaining -= bytes_read;
        p += bytes_read;
    }
    return len - bytes_remaining;
}

uint16_t cdc_get_data_uint16() {
    uint16_t len;
    tud_task();
    cdc_get_bytes((uint8_t *)&len, 2);
    return len;
}

uint8_t cdc_get_data_uint8() {
    uint8_t len;
    tud_task();
    cdc_get_bytes((uint8_t *)&len, 1);
    return len;
}

size_t cdc_get_until(uint8_t *buffer, size_t max_len, uint8_t separator, uint8_t escape, uint timeout_ms = 1000) {
    size_t index = 0;
    bool escaped = false;

    timeout_state ts;
    absolute_time_t until = delayed_by_ms(get_absolute_time(), timeout_ms);
    check_timeout_fn check_timeout = init_single_timeout_until(&ts, until);

    while (index < max_len - 1 && !check_timeout(&ts, until)) {
        tud_task(); // TinyUSB device task

        uint8_t byte;
        if (cdc_get_bytes(&byte, 1) == 0) {
            continue; // No data available, retry until timeout
        }

        if (escaped) {
            // Store the byte literally if it was escaped
            escaped = false;
        } else if (byte == escape) {
            // If escape character is found, set flag and continue to next byte
            escaped = true;
            continue;
        } else if (byte == separator) {
            // End of field reached
            break;
        }

        // Store the byte in the buffer
        buffer[index++] = byte;
    }

    buffer[index] = '\0'; // Null-terminate the string
    return index;
}

void handle_data(const std::string& data) {
    display::info("$> " + data);
}

int main() {
    board_init(); // Wtf?
    usb_serial_init(); // ??
    //cdc_uart_init(); // From cdc_uart.c
    tusb_init(); // Tiny USB?

    std::unordered_map<std::string, std::string> defaults = {
        {"ssid", "MyNetwork"},
        {"pass", "DefaultPass"},
        {"port", "8080"}
    };

    KVStore kvStore(defaults);

    display::init();

    TcpServer server(kvStore.getParam("ssid"), kvStore.getParam("pass"),std::stoi(kvStore.getParam("port")));
    server.set_data_callback(handle_data);

    if (!server.start()) {
        display::info("Failed to start server");
    }


    while (1) {
        tud_task();

        if(!cdc_wait_for("multiverse:")) {
            //display::info("mto");
            continue; // Couldn't get 16 bytes of command
        }

        if(cdc_get_bytes(command_buffer, COMMAND_LEN) != COMMAND_LEN) {
            //display::info("cto");
            continue;
        }

        if(command == "data") {
            if (cdc_get_bytes(display::buffer, display::BUFFER_SIZE) == display::BUFFER_SIZE) {
                display::update();
            }
            continue;
        }

        if(command == "parm") {
            if (cdc_get_until(config_key_buffer, CONFIG_KEY_LEN, ':', '\\')) {
                if (cdc_get_until(config_value_buffer, CONFIG_VALUE_LEN, ':', '\\')) {
                    kvStore.setParam(config_key_buffer, CONFIG_KEY_LEN, config_value_buffer, CONFIG_VALUE_LEN);

                }
            }

            continue;
        }

        if(command=="stor") {
            kvStore.commitToFlash();
            continue;
        }

        if(command == "zdat") {
            // Read the size of the compressed data (4 bytes)
            uint32_t compressed_size;
            if (cdc_get_bytes((uint8_t*)&compressed_size, sizeof(compressed_size)) != sizeof(compressed_size)) {
                // Error handling: failed to read compressed size
                display::info("nosize");
                continue;
            }

            // Ensure compressed_size is within reasonable limits
            const size_t MAX_COMPRESSED_SIZE = display::BUFFER_SIZE;  // Adjust based on expected maximum
            if (compressed_size > MAX_COMPRESSED_SIZE) {
                // Error handling: compressed data too large
                continue;
            }

            // Allocate buffer for compressed data
            uint8_t* compressed_data = (uint8_t*)malloc(compressed_size);
            if (!compressed_data) {
                // Error handling: insufficient memory
                continue;
            }

            // Read the compressed data
            if (cdc_get_bytes(compressed_data, compressed_size) != compressed_size) {
                // Error handling: failed to read compressed data
                free(compressed_data);
                continue;
            }

            // Decompress the data using zlib
            uLongf decompressed_size = display::BUFFER_SIZE;  // Expected size of decompressed data
            int ret = uncompress(display::buffer, &decompressed_size, compressed_data, compressed_size);

            free(compressed_data);  // Free the compressed data buffer

            if (ret != Z_OK) {
                // Error handling: decompression failed
                // Optionally log the error code ret
                continue;
            }

            // Verify that the decompressed size is as expected
            if (decompressed_size != display::BUFFER_SIZE) {
                // Error handling: unexpected decompressed size
                continue;
            }

            // Update the display with the decompressed data
            display::update();

            continue;
        }

        if(command == "_rst") {
            display::info("RST");
            sleep_ms(500);
            save_and_disable_interrupts();
            rosc_hw->ctrl = ROSC_CTRL_ENABLE_VALUE_ENABLE << ROSC_CTRL_ENABLE_LSB;
            watchdog_reboot(0, 0, 0);
            continue;
        }

        if(command == "_usb") {
            display::info("USB");
            sleep_ms(500);
            save_and_disable_interrupts();
            rosc_hw->ctrl = ROSC_CTRL_ENABLE_VALUE_ENABLE << ROSC_CTRL_ENABLE_LSB;
            reset_usb_boot(0, 0);
            continue;
        }
    }

    return 0;
}