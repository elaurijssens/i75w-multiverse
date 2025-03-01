pico_define_board(
        NAME i75w_rp2350
        DESCRIPTION "Custom board i75w with RP2350"
        HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}
        CONFIG_HEADER_FILE i75w_rp2350.h
        SERIAL stdio_uart0
)