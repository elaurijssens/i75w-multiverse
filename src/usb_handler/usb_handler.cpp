#include "usb_handler.hpp"
#include "pico/bootrom.h"
#include "hardware/structs/rosc.h"
#include "hardware/watchdog.h"
#include "pico/timeout_helper.h"
#include "command_config.hpp"
#include "zlib.h"
#include "bsp/board.h"
#include "tusb.h"
#include "cdc_uart.h"
#include "get_serial.h"
#include "matrix.hpp"
#include "config_storage.hpp"
#include "buildinfo.h"
#include "server.hpp"

#define COMMAND_LEN 4
#define CONFIG_KEY_LEN 16
#define CONFIG_VALUE_LEN 128

void usb_serial_write(const std::string& message) {
    if (!tud_cdc_connected()) {
        return;  // Skip if USB is not connected
    }

    tud_cdc_write(message.c_str(), message.length());  // Send the message
    tud_cdc_write("\n", 1);  // Add newline for proper parsing
    tud_cdc_write_flush();  // Ensure data is sent immediately
}

UsbHandler::UsbHandler(KVStore& kvStore, ApiServer& api_server) : kvStore(kvStore), api_server(api_server) {
    usb_serial_init();
    tusb_init();
}

void UsbHandler::start() {
    while (1) {
        tud_task();

        if (!waitFor("multiverse:")) {
            continue;
        }

        uint8_t command_buffer[COMMAND_LEN];
        if (getBytes(command_buffer, COMMAND_LEN) != COMMAND_LEN) {
            continue;
        }

        std::string command(reinterpret_cast<char*>(command_buffer), COMMAND_LEN);
        processCommand(command);
    }
}

void UsbHandler::processCommand(const std::string& command) {
    if (command == CommandConfig::SET) {
        handleSet();
    } else if (command == CommandConfig::GET) {
        handleGet();
    } else if (command == CommandConfig::DELETE) {
        handleDelete();
    } else if (command == CommandConfig::DATA) {
        handleData();
    } else if (command == CommandConfig::ZIPPED) {
        handleZippedData();
    } else if (command == CommandConfig::RESET || command == CommandConfig::BOOTLOADER) {
        handleSystemCommand(command);
    } else if (command == CommandConfig::IPV4) {
        matrix::print("IP: " + api_server.ipv4addr());
    } else if (command == CommandConfig::IPV6) {
        matrix::print("IPV6: " + api_server.ipv6addr());;
    } else if (command == CommandConfig::WRITE) {
        if (kvStore.commitToFlash()) {
            matrix::print("Config written to flash");
        } else {
            matrix::print("Flash already up to date");
        }
    } else  if (command == CommandConfig::USB_DISCOVERY) {
        std::string response = "{"
            "\"width\":" + std::to_string(matrix::WIDTH) + ","
            "\"height\":" + std::to_string(matrix::HEIGHT) + ","
            "\"order\":\"" + kvStore.getParam("color_order") + "\","
            "\"rotation\":" + kvStore.getParam("rotation") + ","
            "\"ip\":\"" + kvStore.getParam("port") + "\","
            "\"port\":" + kvStore.getParam("port") + ","
            "\"build\":\"" + std::string(BUILD_NUMBER) + "\""
        "}";

        usb_serial_write(response);
        return;
    }
}

void UsbHandler::handleSet() {
    uint8_t config_key_buffer[CONFIG_KEY_LEN];
    uint8_t config_value_buffer[CONFIG_VALUE_LEN];

    size_t actual_key_length = getUntil(config_key_buffer, CONFIG_KEY_LEN, ':', '\\');
    if (actual_key_length > 0) {
        size_t actual_value_length = getUntil(config_value_buffer, CONFIG_VALUE_LEN, ':', '\\');
        if (actual_value_length > 0) {
            std::string key(reinterpret_cast<char*>(config_key_buffer), actual_key_length);
            std::string value(reinterpret_cast<char*>(config_value_buffer), actual_value_length);
            kvStore.setParam(key, value);
            matrix::print("Set " + key + " to " + value);
        }
    }
}

void UsbHandler::handleGet() {
    uint8_t config_key_buffer[CONFIG_KEY_LEN];
    size_t actual_key_length = getUntil(config_key_buffer, CONFIG_KEY_LEN, ':', '\\');

    if (actual_key_length > 0) {
        std::string key(reinterpret_cast<char*>(config_key_buffer), actual_key_length);
        matrix::print(key + " = " + kvStore.getParam(key));
    }
}

void UsbHandler::handleDelete() {
    uint8_t config_key_buffer[CONFIG_KEY_LEN];
    size_t actual_key_length = getUntil(config_key_buffer, CONFIG_KEY_LEN, ':', '\\');

    if (actual_key_length > 0) {
        std::string key(reinterpret_cast<char*>(config_key_buffer), actual_key_length);
        if (kvStore.deleteParam(key)) {
            matrix::print("Deleted key: " + key);
        } else {
            matrix::print("Key not found: " + key);
        }
    }
}

void UsbHandler::handleSystemCommand(const std::string& command) {
    if (command == CommandConfig::RESET) {
        matrix::print("RST");
        sleep_ms(500);
        save_and_disable_interrupts();
        rosc_hw->ctrl = ROSC_CTRL_ENABLE_VALUE_ENABLE << ROSC_CTRL_ENABLE_LSB;
        watchdog_reboot(0, 0, 0);
    } else if (command == CommandConfig::BOOTLOADER) {
        matrix::print("USB");
        sleep_ms(500);
        save_and_disable_interrupts();
        rosc_hw->ctrl = ROSC_CTRL_ENABLE_VALUE_ENABLE << ROSC_CTRL_ENABLE_LSB;
        reset_usb_boot(0, 0);
    }
}

void UsbHandler::handleData() {
    if (getBytes(matrix::buffer, matrix::BUFFER_SIZE) == matrix::BUFFER_SIZE) {
        matrix::update();
    }
}

void UsbHandler::handleZippedData() {
    uint32_t compressed_size;
    if (getBytes(reinterpret_cast<uint8_t*>(&compressed_size), sizeof(compressed_size)) != sizeof(compressed_size)) {
        return;
    }

    if (compressed_size > matrix::BUFFER_SIZE) {
        return;
    }

    uint8_t* compressed_data = (uint8_t*)malloc(compressed_size);
    if (!compressed_data) {
        return;
    }

    if (getBytes(compressed_data, compressed_size) != compressed_size) {
        free(compressed_data);
        return;
    }

    uLongf decompressed_size = matrix::BUFFER_SIZE;
    int ret = uncompress(matrix::buffer, &decompressed_size, compressed_data, compressed_size);
    free(compressed_data);

    if (ret == Z_OK && decompressed_size == matrix::BUFFER_SIZE) {
        matrix::update();
    }
}

bool UsbHandler::waitFor(std::string_view data, uint timeout_ms) {
    timeout_state ts;
    absolute_time_t until = delayed_by_ms(get_absolute_time(), timeout_ms);
    check_timeout_fn check_timeout = init_single_timeout_until(&ts, until);

    for (auto expected_char : data) {
        char got_char;
        while (1) {
            tud_task();
            if (cdc_task(reinterpret_cast<uint8_t*>(&got_char), 1) == 1) break;
            if (check_timeout(&ts, until)) return false;
        }
        if (got_char != expected_char) return false;
    }
    return true;
}

size_t UsbHandler::getBytes(uint8_t* buffer, size_t len, uint timeout_ms) {
    memset(buffer, 0, len);
    timeout_state ts;
    absolute_time_t until = delayed_by_ms(get_absolute_time(), timeout_ms);
    check_timeout_fn check_timeout = init_single_timeout_until(&ts, until);

    size_t bytes_remaining = len;
    uint8_t* p = buffer;
    while (bytes_remaining && !check_timeout(&ts, until)) {
        tud_task();
        size_t bytes_read = cdc_task(p, std::min(bytes_remaining, MAX_UART_PACKET));
        bytes_remaining -= bytes_read;
        p += bytes_read;
    }
    return len - bytes_remaining;
}

size_t UsbHandler::getUntil(uint8_t* buffer, size_t max_len, uint8_t separator, uint8_t escape, uint timeout_ms) {
    size_t index = 0;
    bool escaped = false;

    timeout_state ts;
    absolute_time_t until = delayed_by_ms(get_absolute_time(), timeout_ms);
    check_timeout_fn check_timeout = init_single_timeout_until(&ts, until);

    while (index < max_len - 1 && !check_timeout(&ts, until)) {
        tud_task();

        uint8_t byte;
        if (getBytes(&byte, 1) == 0) {
            continue;
        }

        if (escaped) {
            escaped = false;
        } else if (byte == escape) {
            escaped = true;
            continue;
        } else if (byte == separator) {
            break;
        }

        buffer[index++] = byte;
    }

    buffer[index] = '\0';
    return index;
}