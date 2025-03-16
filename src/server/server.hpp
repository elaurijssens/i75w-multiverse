#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <string>
#include <vector>
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/igmp.h"
#include "lwip/ip_addr.h"
#include "config_storage.hpp"  // Include KVStore
#include "lwipopts.h"

constexpr char MESSAGE_PREFIX[] = "multiverse:";  // ✅ Defined prefix
constexpr size_t PREFIX_LENGTH = sizeof(MESSAGE_PREFIX) - 1;  // ✅ Exclude null terminator
constexpr size_t HEADER_SIZE = PREFIX_LENGTH + 8;  // ✅ Includes 4-byte size + 4-char command

class ApiServer {
public:
    explicit ApiServer(KVStore& kvStore);
    ~ApiServer();

    bool start();
    void stop();
    static std::string ipv4addr();
    static std::string ipv6addr();

private:
    KVStore& kvStore;  // Store reference to KVStore
    std::string ssid;
    std::string password;
    std::string multicast_ip;
    uint16_t multicast_port;
    uint16_t port;
    uint16_t brightness;
    uint16_t rotation;
    uint16_t order;
    tcp_pcb* server_pcb;
    std::vector<uint8_t> recv_buffer;
    size_t expected_message_size = 0;

    bool connect_wifi();

    static err_t on_accept(void* arg, struct tcp_pcb* newpcb, err_t err);
    static err_t on_receive(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err);
    static void on_error(void* arg, err_t err);
    static void on_close(struct tcp_pcb* tpcb);

    void run();

    // Private methods for handling data
    static bool process_header(ApiServer* server, uint8_t* payload, size_t data_len);
    static void process_data();
    static void reset_recv_state();
    static void process_key_value_command(ApiServer* server);  // New method to handle `get:`, `set:`, `del:`
    // void udp_recv(struct udp_pcb * pcb, void(TcpServer::* recv)(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port), TcpServer * tcp_server);

    void setup_multicast_listener();
    static void on_multicast_receive(void* arg, struct udp_pcb* upcb, struct pbuf* p, const ip_addr_t* addr, u16_t port);
};

#endif // TCP_SERVER_HPP