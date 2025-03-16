add_library(usb_handler STATIC
        usb_handler.cpp
        cdc_uart.cpp
        get_serial.cpp
        usb_descriptors.cpp
)

target_include_directories(usb_handler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_BINARY_DIR})

target_link_libraries(
        usb_handler
        pico_stdlib
        matrix
        tinyusb_device
        tinyusb_board
        server
)