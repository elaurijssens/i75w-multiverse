#include "tcp_server.hpp"
#include <utility>
#include <cstring>  // For memcmp
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "display.hpp"

TcpServer::TcpServer(const std::string& ssid, const std::string& password, uint16_t port)
    : ssid{ssid}, password{password}, port{port}, server_pcb{nullptr} {}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start() {
    if (!connect_wifi()) {
        display::print("Wi-Fi connection failed");
        return false;
    }

    display::print("Starting TCP server...");
    run();
    return true;
}

void TcpServer::stop() {
    if (server_pcb) {
        tcp_close(server_pcb);
        server_pcb = nullptr;
        display::print("TCP server stopped");
    }
}

std::string TcpServer::ipv4addr() {
    const ip_addr* ip = &netif_list->ip_addr;
    return std::string(ipaddr_ntoa(ip));
}

std::string TcpServer::ipv6addr() {
    std::string ipv6_addresses;

    for (int i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        if (netif_list->ip6_addr_state[i]) {
            if (!ipv6_addresses.empty()) {
                ipv6_addresses += "\n";
            }
            ip_addr* ip = &netif_list->ip6_addr[i];
            ipv6_addresses += std::string(ipaddr_ntoa(ip));
        }
    }

    return ipv6_addresses.empty() ? "No IPv6 address assigned" : ipv6_addresses;
}

void TcpServer::set_binary_callback(BinaryCallback&& callback) {
    binary_callback = std::move(callback);
}

bool TcpServer::connect_wifi() {
    if (cyw43_arch_init()) {
        display::print("Failed to initialize Wi-Fi module");
        return false;
    }

    cyw43_arch_enable_sta_mode();
    display::print("Connecting to Wi-Fi: " + ssid);

    constexpr uint32_t auth_modes[] = {
        CYW43_AUTH_WPA3_SAE_AES_PSK,
        CYW43_AUTH_WPA3_WPA2_AES_PSK,
        CYW43_AUTH_WPA2_MIXED_PSK,
        CYW43_AUTH_WPA2_AES_PSK
    };

    for (uint32_t auth_mode : auth_modes) {
        if (cyw43_arch_wifi_connect_timeout_ms(ssid.c_str(), password.c_str(), auth_mode, 5000) == 0) {
            return true;
        }
    }

    display::print("Unable to connect to Wi-Fi");
    return false;
}

void TcpServer::run() {
    cyw43_arch_lwip_begin();
    server_pcb = tcp_new();

    if (!server_pcb) {
        display::print("Failed to create TCP server PCB");
        cyw43_arch_lwip_end();
        return;
    }

    err_t err = tcp_bind(server_pcb, IP_ADDR_ANY, port);
    if (err != ERR_OK) {
        display::print("Failed to bind TCP server to port " + std::to_string(port));
        cyw43_arch_lwip_end();
        return;
    }

    server_pcb = tcp_listen_with_backlog(server_pcb, 1);
    if (!server_pcb) {
        display::print("Failed to listen on TCP server");
        cyw43_arch_lwip_end();
        return;
    }

    display::print("TCP server listening on port " + std::to_string(port));
    tcp_arg(server_pcb, this);
    tcp_accept(server_pcb, on_accept);
    cyw43_arch_lwip_end();
}

err_t TcpServer::on_accept(void* arg, struct tcp_pcb* newpcb, err_t err) {
    if (err != ERR_OK || !newpcb) {
        display::print("TCP accept error");
        return ERR_VAL;
    }

    TcpServer* server = static_cast<TcpServer*>(arg);
    display::print("Client connected");

    tcp_arg(newpcb, server);
    tcp_recv(newpcb, on_receive);
    tcp_err(newpcb, on_error);

    tcp_nagle_disable(newpcb); // ✅ Prevents unnecessary buffering

    return ERR_OK;
}

#define MAX_BUFFER_SIZE (65 * 1024) // Lower limit for RP2350A memory

err_t TcpServer::on_receive(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err) {
    TcpServer* server = static_cast<TcpServer*>(arg);

    if (!p) {
        display::print("Client disconnected");
        server->recv_buffer.clear();
        tcp_close(tpcb);
        return ERR_OK;
    }

    size_t data_len = p->len;

    // ✅ Prevent buffer overflow
    if (server->recv_buffer.size() + data_len > MAX_BUFFER_SIZE) {
        display::print("Error: Buffer overflow detected, dropping data.");
        pbuf_free(p);
        return ERR_MEM; // Drop excess data instead of crashing
    }

    server->recv_buffer.insert(server->recv_buffer.end(),
        static_cast<uint8_t*>(p->payload),
        static_cast<uint8_t*>(p->payload) + data_len);

    tcp_recved(tpcb, data_len / 2); // ✅ Acknowledge data in smaller chunks

    pbuf_free(p);

    while (server->process_message()) {}

    return ERR_OK;
}

void TcpServer::on_error(void* arg, err_t err) {
    display::print("TCP error: " + std::to_string(err));
}

void TcpServer::on_close(struct tcp_pcb* tpcb) {
    display::print("Closing connection");
    tcp_close(tpcb);
}

bool TcpServer::process_message() {
    if (recv_buffer.size() < 19) return false;

    const std::string prefix = "Multiverse:";
    if (std::memcmp(recv_buffer.data(), prefix.c_str(), prefix.size()) != 0) {
        display::print("Invalid message prefix");
        recv_buffer.clear();
        return false;
    }

    auto file_size = (static_cast<uint32_t>(recv_buffer[11]) << 24) |
                     (static_cast<uint32_t>(recv_buffer[12]) << 16) |
                     (static_cast<uint32_t>(recv_buffer[13]) << 8) |
                     static_cast<uint32_t>(recv_buffer[14]);

    size_t expected_message_size = 19 + file_size;
    if (recv_buffer.size() < expected_message_size) return false;

    std::string command(recv_buffer.begin() + 15, recv_buffer.begin() + 19);
    if (command.size() != 4) {
        display::print("Invalid command size");
        recv_buffer.clear();
        return false;
    }

    // ✅ Process in smaller 512B chunks to prevent blocking
    size_t chunk_size = 1460;
    size_t offset = 19;

    while (offset < expected_message_size) {
        size_t bytes_to_process = std::min(chunk_size, expected_message_size - offset);

        // ✅ Avoid large memory allocations
        std::vector<uint8_t> file_chunk;
        file_chunk.reserve(bytes_to_process);
        file_chunk.insert(file_chunk.end(), recv_buffer.begin() + offset, recv_buffer.begin() + offset + bytes_to_process);

        if (binary_callback) {
            binary_callback(file_chunk);
        }

        offset += bytes_to_process;
    }

    recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + static_cast<std::vector<uint8_t>::difference_type>(expected_message_size));

    display::print("Received command: " + command);
    display::print("Received file size: " + std::to_string(file_size));

    return true;
}