#ifndef CONFIG_STORAGE_HPP
#define CONFIG_STORAGE_HPP

#include "pico/stdlib.h"
#include <string>
#include <unordered_map>

#define FLASH_PAGE_SIZE      256
#define FLASH_SECTOR_SIZE    4096
#define FLASH_KV_STORE_SIZE  FLASH_SECTOR_SIZE
#define FLASH_STORAGE_BASE   (PICO_FLASH_SIZE_BYTES - FLASH_KV_STORE_SIZE)

#define MAX_KEY_LEN    16
#define MAX_VALUE_LEN  128
#define MAX_ENTRIES    26

std::string arrayToString(const uint8_t* data, size_t length);

class KVStore {
public:
    explicit KVStore(const std::unordered_map<std::string, std::string>& defaults = {});

    std::string getParam(const std::string& key);
    std::string getParam(const uint8_t* key, size_t keyLength);
    bool getParam(const uint8_t* key, size_t keyLength, uint8_t* buffer, size_t bufferSize);

    bool setParam(const std::string& key, const std::string& value);
    bool setParam(const uint8_t* key, size_t keyLength, const std::string& value);
    bool setParam(const uint8_t* key, size_t keyLength, const uint8_t* data, size_t length);

    bool deleteParam(const std::string& key);
    bool deleteParam(const uint8_t* key, size_t keyLength);

    bool commitToFlash();
    void loadFromFlash();

private:
    struct kv_pair_t {
        uint8_t key[MAX_KEY_LEN];
        uint8_t keyLength;
        uint8_t value[MAX_VALUE_LEN];
        uint8_t valueLength;
    };

    struct kv_store_t {
        uint32_t valid_flag;
        uint32_t entry_count;
        kv_pair_t entries[MAX_ENTRIES];
        uint32_t crc32;
    };

    kv_store_t kv_store;
    std::unordered_map<std::string, std::string> defaultValues;

    uint32_t calculateCRC32(const uint8_t* data, size_t length);
    bool hasChanged = false;

    bool compareKeys(const uint8_t* key1, size_t len1, const uint8_t* key2, size_t len2);
};

#endif // CONFIG_STORAGE_HPP