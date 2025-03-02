#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <string>
#include <functional>
#include "lwip/tcp.h"
#include "lwip/err.h"
#include "lwipopts.h"

class TcpServer {
public:
    using DataCallback = std::function<void(const std::string&)>;

    TcpServer(const std::string& ssid, const std::string& password, uint16_t port);
    ~TcpServer();

    bool start();
    void stop();
    std::string ipv4addr();
    std::string ipv6addr();
    void set_data_callback(DataCallback callback);

private:
    static err_t on_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
    static err_t on_receive(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
    static void on_error(void *arg, err_t err);
    static void on_close(struct tcp_pcb *tpcb);

    bool connect_wifi();
    void run();

    std::string ssid;
    std::string password;
    uint16_t port;
    struct tcp_pcb *server_pcb;
    DataCallback data_callback;
};

#endif // TCP_SERVER_HPP