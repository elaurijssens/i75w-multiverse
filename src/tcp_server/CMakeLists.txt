add_library(tcp_server STATIC
        tcp_server.cpp
)

target_include_directories(tcp_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
        tcp_server
        pico_stdlib
        display
        pico_cyw43_arch_lwip_threadsafe_background
)