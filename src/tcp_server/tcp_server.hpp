#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <vector>
#include <string>
#include <functional>
#include "lwip/tcp.h"
#include "lwipopts.h"

using BinaryCallback = std::function<void(const std::vector<uint8_t>&)>;

class TcpServer {
public:
    TcpServer(const std::string& ssid, const std::string& password, uint16_t port);
    ~TcpServer();

    bool start();
    void stop();

    void set_binary_callback(BinaryCallback&& callback);

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
    void run();

    static err_t on_accept(void* arg, struct tcp_pcb* newpcb, err_t err);
    static err_t on_receive(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err);
    static void on_error(void* arg, err_t err);
    static void on_close(struct tcp_pcb* tpcb);

    bool process_message();
};

#endif // TCP_SERVER_HPP