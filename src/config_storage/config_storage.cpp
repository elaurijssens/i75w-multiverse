#include "config_storage.hpp"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "buildinfo.h"
#include <cstring>
#include <vector>

// Convert a uint8_t array to a std::string (ASCII-safe conversion)
std::string arrayToString(const uint8_t* data, size_t length) {
    return std::string(reinterpret_cast<const char*>(data), length);
}

// Constructor with defaults
KVStore::KVStore(const std::unordered_map<std::string, std::string>& defaults)
    : defaultValues(defaults) {
    loadFromFlash();
}

// Load data from flash and apply defaults if necessary
void KVStore::loadFromFlash() {
    const uint8_t* flash_mem = (const uint8_t*)(XIP_BASE + FLASH_STORAGE_BASE);
    std::memcpy(&kv_store, flash_mem, sizeof(kv_store_t));

    if (kv_store.valid_flag != 0xDEADBEEF || kv_store.entry_count > MAX_ENTRIES) {
        std::memset(&kv_store, 0, sizeof(kv_store_t));
        kv_store.valid_flag = 0xDEADBEEF;
    }

    uint32_t stored_crc = kv_store.crc32;
    kv_store.crc32 = 0;
    uint32_t calculated_crc = calculateCRC32((uint8_t*)&kv_store, sizeof(kv_store_t));

    if (stored_crc != calculated_crc) {
        std::memset(&kv_store, 0, sizeof(kv_store_t));
        kv_store.valid_flag = 0xDEADBEEF;
    }

    // Apply defaults if missing keys
    for (const auto& [key, value] : defaultValues) {
        if (getParam(reinterpret_cast<const uint8_t*>(key.c_str()), key.length()).empty()) {
            setParam(reinterpret_cast<const uint8_t*>(key.c_str()), key.length(), value);
        }
    }
}

// Commit to flash only if changes are made
bool KVStore::commitToFlash() {
    if (!hasChanged) return false;

    uint32_t ints = save_and_disable_interrupts();
    kv_store.crc32 = 0;
    kv_store.crc32 = calculateCRC32((uint8_t*)&kv_store, sizeof(kv_store_t));

    flash_range_erase(FLASH_STORAGE_BASE, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_STORAGE_BASE, (const uint8_t*)&kv_store, sizeof(kv_store_t));

    restore_interrupts(ints);
    hasChanged = false;

    return true;
}

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

// Compare two binary keys
bool KVStore::compareKeys(const uint8_t* key1, size_t len1, const uint8_t* key2, size_t len2) {
    if (len1 != len2) return false;
    return std::memcmp(key1, key2, len1) == 0;
}

// Retrieve a value using a binary key
std::string KVStore::getParam(const uint8_t* key, size_t keyLength) {
    for (uint32_t i = 0; i < kv_store.entry_count; i++) {
        if (compareKeys(key, keyLength, kv_store.entries[i].key, kv_store.entries[i].keyLength)) {
            return std::string(reinterpret_cast<const char*>(kv_store.entries[i].value), kv_store.entries[i].valueLength);
        }
    }
    return "";
}

// Retrieve a value as a byte array using a binary key
bool KVStore::getParam(const uint8_t* key, size_t keyLength, uint8_t* buffer, size_t bufferSize) {
    for (uint32_t i = 0; i < kv_store.entry_count; i++) {
        if (compareKeys(key, keyLength, kv_store.entries[i].key, kv_store.entries[i].keyLength)) {
            size_t valueSize = std::min(static_cast<size_t>(kv_store.entries[i].valueLength), bufferSize);
            std::memcpy(buffer, kv_store.entries[i].value, valueSize);
            return true;
        }
    }
    return false;
}

std::string KVStore::getParam(const std::string& key) {
    return getParam(reinterpret_cast<const uint8_t*>(key.c_str()), key.length());
}

// Store a string value with a binary key
bool KVStore::setParam(const std::string& key, const std::string& value) {
    return setParam(reinterpret_cast<const uint8_t*>(key.c_str()), key.length(), reinterpret_cast<const uint8_t*>(value.c_str()), value.length());
}

// Store a string value with a binary key
bool KVStore::setParam(const uint8_t* key, size_t keyLength, const std::string& value) {
    return setParam(key, keyLength, reinterpret_cast<const uint8_t*>(value.c_str()), value.length());
}

// Store a binary value with a binary key
bool KVStore::setParam(const uint8_t* key, size_t keyLength, const uint8_t* data, size_t length) {
    if (keyLength > MAX_KEY_LEN || length > MAX_VALUE_LEN) {
        return false;
    }

    // Check if key exists and update it
    for (uint32_t i = 0; i < kv_store.entry_count; i++) {
        if (compareKeys(key, keyLength, kv_store.entries[i].key, kv_store.entries[i].keyLength)) {
            std::memcpy(kv_store.entries[i].value, data, length);
            kv_store.entries[i].valueLength = length;
            hasChanged = true;
            return true;
        }
    }

    // If new, add it
    if (kv_store.entry_count < MAX_ENTRIES) {
        std::memcpy(kv_store.entries[kv_store.entry_count].key, key, keyLength);
        kv_store.entries[kv_store.entry_count].keyLength = keyLength;
        std::memcpy(kv_store.entries[kv_store.entry_count].value, data, length);
        kv_store.entries[kv_store.entry_count].valueLength = length;
        kv_store.entry_count++;
        hasChanged = true;
        return true;
    }

    return false; // Store full
}

bool KVStore::deleteParam(const std::string& key) {
    return deleteParam(reinterpret_cast<const uint8_t*>(key.c_str()), key.length());
}

// Delete a key-value pair using a binary key
bool KVStore::deleteParam(const uint8_t* key, size_t keyLength) {
    for (uint32_t i = 0; i < kv_store.entry_count; i++) {
        if (compareKeys(key, keyLength, kv_store.entries[i].key, kv_store.entries[i].keyLength)) {
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