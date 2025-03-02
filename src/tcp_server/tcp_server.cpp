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
        display::info("Wi-Fi connection failed");
        return false;
    }

    display::info("Starting TCP server...");

    run();
    return true;
}

void TcpServer::stop() {
    if (server_pcb) {
        tcp_close(server_pcb);
        server_pcb = nullptr;
        display::info("TCP server stopped");
    }
}

void TcpServer::set_data_callback(DataCallback callback) {
    data_callback = std::move(callback);
}

bool TcpServer::connect_wifi() {

    if (cyw43_arch_init()) {
        display::info("Failed to initialize Wi-Fi module");
        return false;
    }

    cyw43_arch_enable_sta_mode();
    display::info("Connecting to Wi-Fi: " + ssid);

    if (cyw43_arch_wifi_connect_timeout_ms(ssid.c_str(), password.c_str(), CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        display::info("Wi-Fi connection failed\n");
        return false;
    }

    display::info("Connected to Wi-Fi\n");
    return true;
}

void TcpServer::run() {
    cyw43_arch_lwip_begin();
    server_pcb = tcp_new();

    if (!server_pcb) {
        display::info("Failed to create TCP server PCB\n");
        cyw43_arch_lwip_end();
        return;
    }

    err_t err = tcp_bind(server_pcb, IP_ADDR_ANY, port);
    if (err != ERR_OK) {
        display::info("Failed to bind TCP server to port " + std::to_string(port));
        cyw43_arch_lwip_end();
        return;
    }

    server_pcb = tcp_listen_with_backlog(server_pcb, 1);
    if (!server_pcb) {
        display::info("Failed to listen on TCP server\n");
        cyw43_arch_lwip_end();
        return;
    }

    tcp_arg(server_pcb, this);
    tcp_accept(server_pcb, on_accept);
    cyw43_arch_lwip_end();

    const ip4_addr_t *ip = &netif_list->ip_addr;

    display::info("TCP server listening on ip:port " + std::string(ip4addr_ntoa(ip)) + ":" + std::to_string(port));
}

err_t TcpServer::on_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    if (err != ERR_OK || !newpcb) {
        return ERR_VAL;
    }

    TcpServer *server = static_cast<TcpServer*>(arg);
    display::info("New client connected\n");

    tcp_arg(newpcb, server);
    tcp_recv(newpcb, on_receive);
    tcp_err(newpcb, on_error);

    return ERR_OK;
}

err_t TcpServer::on_receive(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TcpServer *server = static_cast<TcpServer*>(arg);

    if (!p) {
        display::info("Client disconnected\n");
        on_close(tpcb);
        return ERR_OK;
    }

    std::string data((char*)p->payload, p->len);
    display::info("Received data: " + data);

    if (server->data_callback) {
        server->data_callback(data);
    }

    // tcp_write(tpcb, p->payload, p->len, TCP_WRITE_FLAG_COPY);
    pbuf_free(p);
    return ERR_OK;
}

void TcpServer::on_error(void *arg, err_t err) {
    display::info("TCP error:" + std::to_string(err));
}

void TcpServer::on_close(struct tcp_pcb *tpcb) {
    tcp_close(tpcb);
}