#ifndef COMMAND_CONFIG_HPP
#define COMMAND_CONFIG_HPP

#include <string>
#include <unordered_set>

// Define all valid commands here
namespace CommandConfig {
    constexpr char RESET[] = "RSET";
    constexpr char BOOTLOADER[] = "BOOT";
    constexpr char DISCOVERY[] = "dscv";
    constexpr char CLEARSCREEN[] = "clsc";
    constexpr char SYNC[] = "sync";
    constexpr char IPV4[] = "ipv4";
    constexpr char IPV6[] = "ipv6";
    constexpr char WRITE[] = "stor";
    constexpr char GET[] = "kget";
    constexpr char SET[] = "kset";
    constexpr char DELETE[] = "kdel";
    constexpr char SHOWDATA[] = "sdat";
    constexpr char DATA[] = "data";
    constexpr char SHOWZIPPED[] = "szip";
    constexpr char ZIPPED[] = "zipd";

    // Optional: Store as a set for validation or lookup
    const std::unordered_set<std::string> SUPPORTED_COMMANDS = {
        RESET, BOOTLOADER, CLEARSCREEN, SYNC, IPV4, IPV6, WRITE, GET, SET,
        DELETE, DATA, SHOWDATA, SHOWZIPPED, ZIPPED
    };
}

#endif // COMMAND_CONFIG_HPP