add_library(config_storage STATIC
        config_storage.cpp
)

target_include_directories(config_storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
        config_storage
        pico_stdlib
        display
        hardware_flash
)