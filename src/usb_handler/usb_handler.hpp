#ifndef USB_HANDLER_HPP
#define USB_HANDLER_HPP

#include <server.hpp>
#include <string>
#include "config_storage.hpp"
#include "matrix.hpp"

class UsbHandler {
public:
    explicit UsbHandler(KVStore& kvStore, ApiServer& api_server);
    
    void start();
    void processCommand(const std::string& command);

private:
    KVStore& kvStore;
    ApiServer& api_server;

    bool waitFor(std::string_view data, uint timeout_ms = 1000);
    size_t getBytes(uint8_t* buffer, size_t len, uint timeout_ms = 1000);
    size_t getUntil(uint8_t* buffer, size_t max_len, uint8_t separator, uint8_t escape, uint timeout_ms = 1000);

    void handleSet();
    void handleGet();
    void handleDelete();
    void handleData();
    void handleZippedData();
    void handleSystemCommand(const std::string& command);
};

#endif // USB_HANDLER_HPP