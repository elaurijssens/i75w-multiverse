#include "server.hpp"
#include <cstring>

#include "buildinfo.h"
#include "command_config.hpp"

#include "pico/cyw43_arch.h"
#include "pico/bootrom.h"
#include "hardware/structs/rosc.h"
#include "hardware/watchdog.h"
#include "matrix.hpp"
#include "config_storage.hpp"
#include "zlib.h"

struct RecvState {
    size_t expected_size = 0;
    size_t received_size = 0;
    bool receiving_data = false;
    std::string command;
    std::vector<uint8_t> header_buffer;
    std::vector<uint8_t> recv_buffer; // ✅ Reassembly buffer
};

RecvState recv_state;

ApiServer::ApiServer(KVStore &kvStore)
    : kvStore{kvStore}, server_pcb{nullptr} {
    ssid = kvStore.getParam("ssid");
    password = kvStore.getParam("pass");
    multicast_ip = kvStore.getParam("mcast_ip"); // Define a multicast IP

    // Helper lambda to convert string to int safely
    auto safe_stoi = [](const std::string &str, int default_value, int min_val, int max_val) -> int {
        if (str.empty()) return default_value;
        int value = 0;
        for (char c: str) {
            if (!isdigit(c)) return default_value; // Ensure only digits are present
        }
        value = std::atoi(str.c_str());
        return (value >= min_val && value <= max_val) ? value : default_value;
    };

    port = safe_stoi(kvStore.getParam("port"), 54321, 0, 65535);
    multicast_port = safe_stoi(kvStore.getParam("mcast_port"), 54321, 0, 65535);
    rotation = safe_stoi(kvStore.getParam("rotation"), 0, 0, 270);
    order = safe_stoi(kvStore.getParam("order"), 1, 0, 65535);
    brightness = safe_stoi(kvStore.getParam("brightness"), 127, 0, 255);

    // Ensure rotation is only 0, 90, 180, or 270
    if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270) {
        rotation = 0;
    }
}

ApiServer::~ApiServer() {
    stop();
}

bool ApiServer::start() {
    if (!connect_wifi()) {
        DEBUG_PRINT("Wi-Fi connection failed");
        return false;
    }

    DEBUG_PRINT("Starting multicast listener...");

    setup_multicast_listener();

    DEBUG_PRINT("Starting TCP server...");
    run();
    return true;
}

void ApiServer::stop() {
    if (server_pcb) {
        tcp_close(server_pcb);
        server_pcb = nullptr;
        DEBUG_PRINT("TCP server stopped");
    }
}

std::string ApiServer::ipv4addr() {
    const ip_addr *ip = &netif_list->ip_addr;
    return ipaddr_ntoa(ip);
}

std::string ApiServer::ipv6addr() {
    std::string ipv6_addresses;
    for (int i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        if (netif_list->ip6_addr_state[i]) {
            if (!ipv6_addresses.empty()) {
                ipv6_addresses += "\n";
            }
            ip_addr *ip = &netif_list->ip6_addr[i];
            ipv6_addresses += std::string(ipaddr_ntoa(ip));
        }
    }
    return ipv6_addresses.empty() ? "No IPv6 address assigned" : ipv6_addresses;
}

// void ApiServer::set_binary_callback(BinaryCallback&& callback) {
//     binary_callback = std::move(callback);
// }

bool ApiServer::connect_wifi() {
    DEBUG_PRINT("Attempting to connect to Wi-Fi...");
    if (cyw43_arch_init()) {
        matrix::print("Failed to initialize Wi-Fi module");
        return false;
    }

    cyw43_arch_enable_sta_mode();
    matrix::print("Connecting to Wi-Fi: " + ssid);

    // Retrieve last successful auth mode from kvStore (default to WPA2 AES if not found)
    std::string stored_auth_mode_str = kvStore.getParam("wifi_auth");
    uint32_t stored_auth_mode = std::stoi(stored_auth_mode_str);

    constexpr uint32_t auth_modes[] = {
        CYW43_AUTH_WPA3_SAE_AES_PSK,
        CYW43_AUTH_WPA3_WPA2_AES_PSK,
        CYW43_AUTH_WPA2_MIXED_PSK,
        CYW43_AUTH_WPA2_AES_PSK
    };

    // Try stored auth mode first
    if (std::find(std::begin(auth_modes), std::end(auth_modes), stored_auth_mode) != std::end(auth_modes)) {
        DEBUG_PRINT("Trying stored auth mode: " + std::to_string(stored_auth_mode));
        if (cyw43_arch_wifi_connect_timeout_ms(ssid.c_str(), password.c_str(), stored_auth_mode, 5000) == 0) {
            return true;
        }
    }

    // If stored mode fails, try all modes in order
    for (int retry = 1; retry < 4; retry++) {
        for (uint32_t auth_mode: auth_modes) {
            if (auth_mode == stored_auth_mode) continue; // Skip stored mode (already tried)

            if (cyw43_arch_wifi_connect_timeout_ms(ssid.c_str(), password.c_str(), auth_mode, 2000 * retry) == 0) {
                // ✅ If auth mode is different from stored, update kvStore
                {
                    kvStore.setParam("wifi_auth", std::to_string(auth_mode));
                    kvStore.commitToFlash();
                    DEBUG_PRINT("Updated auth mode: " + std::to_string(auth_mode));
                }
                return true;
            }
        }
    }

    matrix::print("Unable to connect to Wi-Fi");
    return false;
}

void ApiServer::run() {
    cyw43_arch_lwip_begin();
    server_pcb = tcp_new();

    if (!server_pcb) {
        DEBUG_PRINT("Failed to create TCP server PCB");
        cyw43_arch_lwip_end();
        return;
    }

    err_t err = tcp_bind(server_pcb, IP_ADDR_ANY, port);
    if (err != ERR_OK) {
        DEBUG_PRINT("Failed to bind TCP server to port " + std::to_string(port));
        cyw43_arch_lwip_end();
        return;
    }

    server_pcb = tcp_listen_with_backlog(server_pcb, 1);
    if (!server_pcb) {
        DEBUG_PRINT("Failed to listen on TCP server");
        cyw43_arch_lwip_end();
        return;
    }

    DEBUG_PRINT("TCP server listening on port " + std::to_string(port));
    tcp_arg(server_pcb, this);
    tcp_accept(server_pcb, on_accept);
    cyw43_arch_lwip_end();
}

err_t ApiServer::on_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    if (err != ERR_OK || !newpcb) {
        DEBUG_PRINT("TCP accept error");
        return ERR_VAL;
    }

    auto *server = static_cast<ApiServer *>(arg); // ✅ Retrieve ApiServer instance
    DEBUG_PRINT("Client connected");

    tcp_arg(newpcb, server); // ✅ Store server instance in the connection
    tcp_recv(newpcb, ApiServer::on_receive);
    tcp_err(newpcb, ApiServer::on_error);

    return ERR_OK;
}

#define MAX_BUFFER_SIZE (65 * 1024)  // ✅ Prevents memory overflow

err_t ApiServer::on_receive(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    auto *server = static_cast<ApiServer *>(arg);

    if (!p) {
        DEBUG_PRINT("Client disconnected");
        server->reset_recv_state();
        tcp_close(tpcb);
        return ERR_OK;
    }

    size_t data_len = p->len;
    uint8_t *payload = static_cast<uint8_t *>(p->payload);
    pbuf_free(p);

    size_t offset = 0; // ✅ Use an explicit offset instead of modifying payload

    if (!recv_state.receiving_data) {
        if (!server->process_header(server, payload, data_len)) {
            return ERR_OK;
        }
        offset = HEADER_SIZE; // ✅ Move past header
    }

    // ✅ Prevent buffer overflow
    if (recv_state.recv_buffer.size() + (data_len - offset) > MAX_BUFFER_SIZE) {
        DEBUG_PRINT("Error: Buffer overflow detected, dropping data.");
        recv_state.recv_buffer.clear();
        return ERR_MEM;
    }

    // ✅ Store data in reassembly buffer
    recv_state.recv_buffer.insert(recv_state.recv_buffer.end(), payload + offset, payload + data_len);

    tcp_recved(tpcb, data_len); // ✅ Acknowledge full data received

    // ✅ Immediately process key-value commands
    if (recv_state.command == CommandConfig::GET || recv_state.command == CommandConfig::SET || recv_state.command ==
        CommandConfig::DELETE) {
        if (recv_state.recv_buffer.size() >= recv_state.expected_size) {
            server->process_key_value_command(server);
            recv_state.receiving_data = false;
            recv_state.recv_buffer.clear(); // ✅ Clear buffer after processing
        }
        return ERR_OK;
    }

    // ✅ Process image data only after all chunks are received
    DEBUG_PRINT(
        "Buffer: "+ std::to_string(recv_state.recv_buffer.size())+"  Expected: " + std::to_string( recv_state.
            expected_size));
    if (recv_state.recv_buffer.size() >= recv_state.expected_size) {
        server->process_data();
        recv_state.receiving_data = false;
    }

    return ERR_OK;
}

bool ApiServer::process_header(ApiServer *server, uint8_t *payload, size_t data_len) {
    recv_state.header_buffer.insert(recv_state.header_buffer.end(), payload, payload + data_len);

    if (recv_state.header_buffer.size() < HEADER_SIZE) {
        return false;
    }

    uint8_t *header_data = recv_state.header_buffer.data();
    std::string header_prefix(reinterpret_cast<char *>(header_data), PREFIX_LENGTH);

    if (header_prefix != MESSAGE_PREFIX) {
        DEBUG_PRINT("Invalid message prefix: " + header_prefix);
        recv_state.header_buffer.clear();
        return false;
    }

    recv_state.expected_size = (header_data[PREFIX_LENGTH] << 24) |
                               (header_data[PREFIX_LENGTH + 1] << 16) |
                               (header_data[PREFIX_LENGTH + 2] << 8) |
                               header_data[PREFIX_LENGTH + 3];

    recv_state.command = std::string(reinterpret_cast<char *>(header_data + PREFIX_LENGTH + 4), 4);


    if (CommandConfig::SUPPORTED_COMMANDS.find(recv_state.command) == CommandConfig::SUPPORTED_COMMANDS.end()) {
        DEBUG_PRINT("Unknown command: " + recv_state.command);
        return false;
    }

    recv_state.received_size = 0;
    recv_state.receiving_data = (recv_state.command == CommandConfig::DATA ||
                                 recv_state.command == CommandConfig::SHOWDATA ||
                                 recv_state.command == CommandConfig::ZIPPED ||
                                 recv_state.command == CommandConfig::SHOWZIPPED ||
                                 recv_state.command == CommandConfig::PRINT); // ✅ New case for `prnt`

    DEBUG_PRINT("Received command: " + recv_state.command);
    recv_state.header_buffer.clear();

    if (recv_state.command == CommandConfig::GET || recv_state.command == CommandConfig::SET || recv_state.command ==
        CommandConfig::DELETE) {
        recv_state.receiving_data = true;
        return true; // Indicate that more data is expected
    }

    if (recv_state.command == CommandConfig::RESET) {
        matrix::print("Resetting...");
        sleep_ms(500);
        save_and_disable_interrupts();
        rosc_hw->ctrl = ROSC_CTRL_ENABLE_VALUE_ENABLE << ROSC_CTRL_ENABLE_LSB;
        watchdog_reboot(0, 0, 0);
        return false;
    } else if (recv_state.command == CommandConfig::BOOTLOADER) {
        matrix::print("Entering BOOTSEL mode...");
        sleep_ms(500);
        save_and_disable_interrupts();
        rosc_hw->ctrl = ROSC_CTRL_ENABLE_VALUE_ENABLE << ROSC_CTRL_ENABLE_LSB;
        reset_usb_boot(0, 0);
        return false;
    } else if (recv_state.command == CommandConfig::FACTORY_RESET) {
        matrix::print("Factory resetting...");
        server->kvStore.setFactoryDefaults();
        DEBUG_PRINT("Factory reset");
        sleep_ms(500);
        save_and_disable_interrupts();
        rosc_hw->ctrl = ROSC_CTRL_ENABLE_VALUE_ENABLE << ROSC_CTRL_ENABLE_LSB;
        watchdog_reboot(0, 0, 0);
        return false;
    } else if (recv_state.command == CommandConfig::CLEARSCREEN) {
        matrix::clearscreen();
        DEBUG_PRINT("Cleared display");
        return false;
    } else if (recv_state.command == CommandConfig::SYNC) {
        matrix::update();
        DEBUG_PRINT("Display synchronized");
        return false;
    } else if (recv_state.command == CommandConfig::IPV4) {
        matrix::print(ipv4addr());
        return false;
    } else if (recv_state.command == CommandConfig::IPV6) {
        matrix::print(ipv6addr());
        return false;
    } else if (recv_state.command == CommandConfig::WRITE) {
        matrix::print("Storing key-value store...");
        server->kvStore.commitToFlash();
        return false;
    }

    return recv_state.receiving_data;
}

void ApiServer::process_data() {
    if (recv_state.recv_buffer.empty()) {
        DEBUG_PRINT("Error: Received empty data buffer!");
        return;
    }

    DEBUG_PRINT("Processing data bytes: " + std::to_string(recv_state.recv_buffer.size()));

    if (recv_state.command == CommandConfig::DATA || recv_state.command == CommandConfig::SHOWDATA) {
        // ✅ Standard uncompressed data handling
        size_t copy_size = std::min(recv_state.recv_buffer.size(), matrix::BUFFER_SIZE);
        std::memcpy(matrix::buffer, recv_state.recv_buffer.data(), copy_size);
    } else if (recv_state.command == CommandConfig::ZIPPED || recv_state.command == CommandConfig::SHOWZIPPED) {
        // ✅ Decompression handling
        uLongf dest_len = matrix::BUFFER_SIZE; // Maximum allowed decompressed size
        int result = uncompress(matrix::buffer, &dest_len, recv_state.recv_buffer.data(),
                                recv_state.recv_buffer.size());

        if (result != Z_OK) {
            DEBUG_PRINT("Error: Decompression failed with code " + std::to_string(result));
            return;
        }

        DEBUG_PRINT("Decompressed size: " + std::to_string(dest_len));
    } else if (recv_state.command == CommandConfig::PRINT) {
        // ✅ Limit received text to 1024 characters
        size_t copy_size = std::min(recv_state.recv_buffer.size(), static_cast<size_t>(1024));

        // ✅ Convert received data to a string
        std::string raw_message(reinterpret_cast<const char *>(recv_state.recv_buffer.data()), copy_size);

        // ✅ Filter out non-printable ASCII characters
        std::string filtered_message;
        for (char c: raw_message) {
            if (c >= 32 && c <= 126) {
                // ✅ Keep only printable ASCII
                filtered_message += c;
            }
        }

        if (filtered_message.empty()) {
            DEBUG_PRINT("eceived only non-printable characters, ignoring.");
            return;
        }

        // ✅ Print the filtered message on the display
        matrix::print(filtered_message);

        DEBUG_PRINT("Displayed filtered text");
    }

    if (recv_state.command == CommandConfig::SHOWDATA || recv_state.command == CommandConfig::SHOWZIPPED) {
        matrix::update();
        DEBUG_PRINT("Image received and updated");
    } else {
        DEBUG_PRINT("Image received (waiting for sync)");
    }

    recv_state.recv_buffer.clear(); // ✅ Clear buffer after processing
}

void ApiServer::process_key_value_command(ApiServer *server) {
    DEBUG_PRINT("Processing key-value command");

    if (recv_state.recv_buffer.empty()) {
        matrix::print("Error: Received empty key-value buffer!");
        return;
    }

    std::string data(reinterpret_cast<char *>(recv_state.recv_buffer.data()), recv_state.recv_buffer.size());

    DEBUG_PRINT("Received data: " + data);

    size_t delimiter = data.find(':');
    if (delimiter == std::string::npos) {
        matrix::print("Malformed key-value command");
        return;
    }

    std::string key = data.substr(0, delimiter);
    std::string value = data.substr(delimiter + 1);

    if (recv_state.command == CommandConfig::GET) {
        std::string retrieved_value = server->kvStore.getParam(key);
        matrix::print("Get " + key + ": " + retrieved_value);
    } else if (recv_state.command == CommandConfig::SET) {
        matrix::print("Set " + key + " to " + value);
        server->kvStore.setParam(key, value);
    } else if (recv_state.command == CommandConfig::DELETE) {
        matrix::print("Deleting key: " + key);
        server->kvStore.deleteParam(key);
    }
}

void ApiServer::reset_recv_state() {
    recv_state.receiving_data = false;
    recv_state.expected_size = 0;
    recv_state.received_size = 0;
    recv_state.command.clear();
    recv_state.header_buffer.clear();
}

void ApiServer::on_error(void *arg, err_t err) {
    DEBUG_PRINT("TCP error: " + std::to_string(err));
}

struct udp_pcb *udp_sync_pcb = nullptr;

void ApiServer::setup_multicast_listener() {
    udp_sync_pcb = udp_new();
    if (!udp_sync_pcb) {
        matrix::print("Failed to create UDP multicast PCB");
        return;
    }

    ip4_addr_t multicast_addr;
    ip4addr_aton(multicast_ip.c_str(), &multicast_addr);

    // ✅ Explicitly JOIN the multicast group
    err_t err = igmp_joingroup(ip_2_ip4(IP_ADDR_ANY), &multicast_addr);
    if (err != ERR_OK) {
        matrix::print("Failed to join multicast group");
        return;
    }

    err = udp_bind(udp_sync_pcb, IP_ADDR_ANY, multicast_port);
    if (err != ERR_OK) {
        matrix::print("Failed to bind UDP multicast listener");
        return;
    }

    udp_recv(udp_sync_pcb, &ApiServer::on_multicast_receive, this);

    matrix::print("Listening for multicast sync on " + multicast_ip + ":" + std::to_string(multicast_port));
}

void ApiServer::on_multicast_receive(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr,
                                     u16_t port) {
    if (!p) return;

    // ✅ Get the actual ApiServer instance from `arg`
    ApiServer *server = static_cast<ApiServer *>(arg);

    DEBUG_PRINT("Received multicast data");

    std::string received_data(static_cast<char *>(p->payload), p->len);
    pbuf_free(p);

    if (received_data == CommandConfig::SYNC) {
        matrix::update();
        DEBUG_PRINT("Sync command received via multicast");
    } else if (received_data == CommandConfig::DISCOVERY) {
        // ✅ New discovery feature
        matrix::print("Discovery request received");

        // ✅ Access kvStore via `server->kvStore`
        std::string response = R"({ "width": )" + std::to_string(matrix::WIDTH) + R"(, )" +
                               R"("height": )" + std::to_string(matrix::HEIGHT) + R"(, )" +
                               R"("rotation": )" + std::to_string(server->rotation) + R"(, )" +
                               R"("order": )" + std::to_string(server->order) + R"(, )" +
                               R"("ip_address": ")" + server->ipv4addr() + R"(", )" +
                               R"("port": )" + std::to_string(server->port) + R"(, )" +
                               R"("build": ")" + BUILD_NUMBER + R"(" })";

        pbuf *response_pbuf = pbuf_alloc(PBUF_TRANSPORT, response.size(), PBUF_RAM);
        if (!response_pbuf) {
            DEBUG_PRINT("Failed to allocate pbuf for multicast response");
            return; // ✅ Avoid using a null pointer
        }

        memcpy(response_pbuf->payload, response.c_str(), response.size());

        // ✅ Ensure `pbuf_free(response_pbuf)` is always called
        err_t send_err = udp_sendto(upcb, response_pbuf, addr, port);
        pbuf_free(response_pbuf); // ✅ Always free the buffer

        if (send_err != ERR_OK) {
            DEBUG_PRINT("Failed to send multicast response, error: " + std::to_string(send_err));
        }

        matrix::print("Sent discovery response: " + response);
    }
}
