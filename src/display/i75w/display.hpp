#pragma once

#include "libraries/pico_graphics/pico_graphics.hpp"
#include "libraries/interstate75/interstate75.hpp"


namespace display {
    const int WIDTH = 256;
    const int HEIGHT = 64;
    const size_t BUFFER_SIZE = WIDTH * HEIGHT * 4;

    void init();
    void update();
    void clear();
    void info(std::string text);
    void print(std::string text, bool append = false);
    extern uint8_t buffer[BUFFER_SIZE];
}