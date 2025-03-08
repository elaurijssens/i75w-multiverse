#include "tcp_server.hpp"
#include <cstdio>
#include <utility>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "display.hpp"

TcpServer::TcpServer(const std::string& ssid, const std::string& password, uint16_t port)
    : ssid(ssid), password(password), port(port), server_pcb(nullptr) {}

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
    const ip_addr *ip = &netif_list->ip_addr;
    return std::string(ipaddr_ntoa(ip));
}

std::string TcpServer::ipv6addr() {
    std::string ipv6_addresses;

    for (int i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        if (netif_list->ip6_addr_state[i]) { // Check if IPv6 address is valid
            if (!ipv6_addresses.empty()) {
                ipv6_addresses += "\n";  // Add newline separator
            }
            ip_addr *ip = &netif_list->ip6_addr[i];
            ipv6_addresses += std::string(ipaddr_ntoa(ip));
        }
    }

    return ipv6_addresses.empty() ? "No IPv6 address assigned" : ipv6_addresses;
}

void TcpServer::set_data_callback(DataCallback callback) {
    data_callback = std::move(callback);
}

void TcpServer::set_binary_callback(BinaryCallback callback) {
    binary_callback = callback;
}

bool TcpServer::connect_wifi() {

    if (cyw43_arch_init()) {
        display::print("Failed to initialize Wi-Fi module");
        return false;
    }

    cyw43_arch_enable_sta_mode();
    display::print("Connecting to Wi-Fi: " + ssid);

    const uint32_t auth_modes[] = {
        CYW43_AUTH_WPA3_SAE_AES_PSK,    // WPA3
        CYW43_AUTH_WPA3_WPA2_AES_PSK,
        CYW43_AUTH_WPA2_MIXED_PSK,  // WPA2/WPA3 Mixed Mode
        CYW43_AUTH_WPA2_AES_PSK     // WPA2
    };

    for (uint32_t auth_mode : auth_modes) {

        if (cyw43_arch_wifi_connect_timeout_ms(ssid.c_str(), password.c_str(), auth_mode, 5000) == 0) {
            return true;
        }
    }

    display::print("Unable to connect to Wi-Fi\n");
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

err_t TcpServer::on_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    if (err != ERR_OK || !newpcb) {
        display::print("TCP accept error");
        return ERR_VAL;
    }

    TcpServer *server = static_cast<TcpServer*>(arg);
    display::print("Client connected");

    tcp_arg(newpcb, server);
    tcp_recv(newpcb, on_receive);
    tcp_err(newpcb, on_error);

    return ERR_OK;
}

err_t TcpServer::on_receive(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TcpServer *server = static_cast<TcpServer*>(arg);

    if (!p) {
        display::print("Client disconnected");
        on_close(tpcb);
        return ERR_OK;
    }

    std::string data((char*)p->payload, p->len);
    display::print("Received data len: " + std::to_string(p->len));

    if (server->data_callback) {
        server->data_callback(data);
    }

    tcp_write(tpcb, p->payload, p->len, TCP_WRITE_FLAG_COPY);
    pbuf_free(p);
    return ERR_OK;
}

void TcpServer::on_error(void *arg, err_t err) {
    display::print("TCP error: " + std::to_string(err));
}

void TcpServer::on_close(struct tcp_pcb *tpcb) {
    display::print("Closing connection");
    tcp_close(tpcb);
}
