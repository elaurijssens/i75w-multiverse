#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <functional>
#include <string>
#include <vector>
#include "lwip/tcp.h"
#include "lwipopts.h"

using BinaryCallback = std::function<void(const std::vector<uint8_t>&)>;

class TcpServer {
public:
    TcpServer(const std::string& ssid, const std::string& password, uint16_t port);
    ~TcpServer();

    bool start();
    void stop();

    static std::string ipv4addr();
    static std::string ipv6addr();

private:
    std::string ssid;
    std::string password;
    uint16_t port;
    struct tcp_pcb* server_pcb;
    BinaryCallback binary_callback;
    std::vector<uint8_t> recv_buffer;
    size_t expected_message_size = 0;

    bool connect_wifi();

    static err_t on_accept(void* arg, struct tcp_pcb* newpcb, err_t err);
    static err_t on_receive(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err);
    static void on_error(void* arg, err_t err);
    static void on_close(struct tcp_pcb* tpcb);

    void run();

    // New private methods for structured handling
    static bool process_header(uint8_t* payload, size_t data_len);
    static void process_data(uint8_t* payload, size_t data_len);
    static void reset_recv_state();
};

#endif // TCP_SERVER_HPP