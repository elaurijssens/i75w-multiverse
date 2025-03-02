#include "config_storage.hpp"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <cstring>
#include <iomanip>

// CRC32 function
uint32_t KVStore::calculateCRC32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 * (crc & 1));
        }
    }
    return ~crc;
}

// Constructor: Initialize defaults and load flash
KVStore::KVStore(const std::unordered_map<std::string, std::string>& defaults) : defaultValues(defaults) {
    loadFromFlash();
}

// Load the key-value store from flash and validate
void KVStore::loadFromFlash() {
    const uint8_t* flash_mem = (const uint8_t*)(XIP_BASE + FLASH_STORAGE_BASE);
    std::memcpy(&kv_store, flash_mem, sizeof(kv_store_t));

    // Validate magic number and entry count
    if (kv_store.valid_flag != 0xDEADBEEF || kv_store.entry_count > MAX_ENTRIES) {
        std::memset(&kv_store, 0, sizeof(kv_store_t));
        kv_store.valid_flag = 0xDEADBEEF;
    }

    // Validate CRC32
    uint32_t stored_crc = kv_store.crc32;
    kv_store.crc32 = 0;
    uint32_t calculated_crc = calculateCRC32((uint8_t*)&kv_store, sizeof(kv_store_t));

    if (stored_crc != calculated_crc) {
        std::memset(&kv_store, 0, sizeof(kv_store_t));
        kv_store.valid_flag = 0xDEADBEEF;
    }

    // Apply default values if needed
    for (const auto& [key, value] : defaultValues) {
        if (getParam(key).empty()) {
            setParam(key, value);
        }
    }
}

// Commit to flash only if data has changed
void KVStore::commitToFlash() {
    if (!hasChanged) return;

    uint32_t ints = save_and_disable_interrupts();
    kv_store.crc32 = 0;
    kv_store.crc32 = calculateCRC32((uint8_t*)&kv_store, sizeof(kv_store_t));

    flash_range_erase(FLASH_STORAGE_BASE, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_STORAGE_BASE, (const uint8_t*)&kv_store, sizeof(kv_store_t));

    restore_interrupts(ints);
    hasChanged = false;
}

// Retrieve a value (with fallback to defaults)
// Retrieve a value as a string
std::string KVStore::getParam(const std::string& key) {
    for (uint32_t i = 0; i < kv_store.entry_count; i++) {
        if (key == kv_store.entries[i].key) {
            return std::string(kv_store.entries[i].value);
        }
    }
    if (defaultValues.find(key) != defaultValues.end()) {
        return defaultValues[key];
    }
    return "";
}

// Retrieve a value as a byte array
bool KVStore::getParam(const std::string& key, uint8_t* buffer, size_t bufferSize) {
    for (uint32_t i = 0; i < kv_store.entry_count; i++) {
        if (key == kv_store.entries[i].key) {
            size_t valueSize = std::min(strlen(kv_store.entries[i].value), bufferSize);
            std::memcpy(buffer, kv_store.entries[i].value, valueSize);
            return true;
        }
    }
    return false;
}

// Helper function: Convert uint8_t[] key to std::string (Hex format)
std::string keyToString(const uint8_t* key, size_t keyLength) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < keyLength; i++) {
        ss << std::setw(2) << static_cast<int>(key[i]);
    }
    return ss.str();
}

// Store a string value with a binary key
bool KVStore::setParam(const uint8_t* key, size_t keyLength, const std::string& value) {
    if (keyLength >= MAX_KEY_LEN || value.length() >= MAX_VALUE_LEN) {
        return false;
    }
    return setParam(keyToString(key, keyLength), value);
}

// Store a binary value with a binary key
bool KVStore::setParam(const uint8_t* key, size_t keyLength, const uint8_t* data, size_t length) {
    if (keyLength >= MAX_KEY_LEN || length >= MAX_VALUE_LEN) {
        return false;
    }
    return setParam(keyToString(key, keyLength), data, length);
}


// Store a string value
bool KVStore::setParam(const std::string& key, const std::string& value) {
    if (key.length() >= MAX_KEY_LEN || value.length() >= MAX_VALUE_LEN) {
        return false;
    }

    for (uint32_t i = 0; i < kv_store.entry_count; i++) {
        if (key == kv_store.entries[i].key) {
            if (kv_store.entries[i].value == value) return true;
            std::strncpy(kv_store.entries[i].value, value.c_str(), MAX_VALUE_LEN);
            hasChanged = true;
            return true;
        }
    }

    if (kv_store.entry_count < MAX_ENTRIES) {
        std::strncpy(kv_store.entries[kv_store.entry_count].key, key.c_str(), MAX_KEY_LEN);
        std::strncpy(kv_store.entries[kv_store.entry_count].value, value.c_str(), MAX_VALUE_LEN);
        kv_store.entry_count++;
        hasChanged = true;
        return true;
    }

    return false;
}

// Store a binary value (byte array)
bool KVStore::setParam(const std::string& key, const uint8_t* data, size_t length) {
    if (key.length() >= MAX_KEY_LEN || length >= MAX_VALUE_LEN) {
        return false;
    }

    for (uint32_t i = 0; i < kv_store.entry_count; i++) {
        if (key == kv_store.entries[i].key) {
            std::memcpy(kv_store.entries[i].value, data, length);
            kv_store.entries[i].value[length] = '\0';  // Ensure null termination
            hasChanged = true;
            return true;
        }
    }

    if (kv_store.entry_count < MAX_ENTRIES) {
        std::strncpy(kv_store.entries[kv_store.entry_count].key, key.c_str(), MAX_KEY_LEN);
        std::memcpy(kv_store.entries[kv_store.entry_count].value, data, length);
        kv_store.entries[kv_store.entry_count].value[length] = '\0';
        kv_store.entry_count++;
        hasChanged = true;
        return true;
    }

    return false;
}
// Delete a key-value pair
bool KVStore::deleteParam(const std::string& key) {
    for (uint32_t i = 0; i < kv_store.entry_count; i++) {
        if (key == kv_store.entries[i].key) {
            for (uint32_t j = i; j < kv_store.entry_count - 1; j++) {
                kv_store.entries[j] = kv_store.entries[j + 1];
            }
            kv_store.entry_count--;
            hasChanged = true;
            return true;
        }
    }
    return false;
}