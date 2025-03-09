add_library(display INTERFACE)

include(${PIMORONI_PICO_PATH}/libraries/interstate75/interstate75.cmake)

target_sources(display INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/display.cpp
)

target_include_directories(display INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(display INTERFACE

        interstate75
        pico_graphics
        hershey_fonts
        bitmap_fonts
        config_storage

        pico_stdlib
        hardware_adc
        hardware_pio
        hardware_dma
)

set(DISPLAY_NAME "Interstate 75w")

# Let CMake know about the selected board
message(STATUS "Building for board: ${PICO_BOARD}")