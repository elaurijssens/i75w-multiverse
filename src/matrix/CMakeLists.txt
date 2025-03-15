add_library(matrix STATIC
        matrix.cpp
)

target_include_directories(matrix PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

include(${PIMORONI_PICO_PATH}/libraries/interstate75/interstate75.cmake)
include_directories(${CMAKE_BINARY_DIR})

target_link_libraries(
        matrix

        interstate75
        hub75
        pico_graphics
        hershey_fonts
        bitmap_fonts
        config_storage

        pico_stdlib
        hardware_adc
        hardware_pio
        hardware_dma
        config_storage
        pico_stdlib

)

