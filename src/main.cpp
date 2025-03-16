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

#include "matrix.hpp"
#include "pico/bootrom.h"
#include "server.hpp"
#include "config_storage.hpp"
#include "usb_handler.hpp"

const size_t COMMAND_LEN = 4;
const size_t CONFIG_KEY_LEN = 16;
const size_t CONFIG_VALUE_LEN = 128;
uint8_t command_buffer[COMMAND_LEN];
uint8_t config_key_buffer[CONFIG_KEY_LEN];
uint8_t config_value_buffer[CONFIG_VALUE_LEN];

std::string_view command((const char *)command_buffer, COMMAND_LEN);


int main() {

    KVStore kvStore;

    matrix::init(kvStore);

    ApiServer server(kvStore);
    UsbHandler usbHandler(kvStore, server);

    if (!server.start()) {
        matrix::print("Failed to start TCP server");
    } else {
        matrix::print("TCP server started on " + server.ipv4addr() + ":" + kvStore.getParam("port"));
    }
    usbHandler.start();

    return 0;
}