add_library(config_storage STATIC
        config_storage.cpp
)

target_include_directories(config_storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_BINARY_DIR})

target_link_libraries(
        config_storage
        pico_stdlib
        matrix
        hardware_flash
)