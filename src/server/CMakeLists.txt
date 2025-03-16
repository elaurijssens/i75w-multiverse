add_library(server STATIC
        server.cpp
        command_config.hpp
)

target_include_directories(server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_BINARY_DIR})

target_link_libraries(
        server
        pico_stdlib
        matrix
        pico_cyw43_arch_lwip_threadsafe_background
        zlib
        config_storage
)