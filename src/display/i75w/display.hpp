#pragma once

#include "libraries/pico_graphics/pico_graphics.hpp"
#include "libraries/interstate75/interstate75.hpp"


namespace display {
    const int WIDTH = 256;
    const int HEIGHT = 64;
    const size_t BUFFER_SIZE = WIDTH * HEIGHT * 4;

    void init();
    void update();
    void info(std::string_view text);
    void play_audio(uint8_t *audio_buffer, size_t len);
    void play_note(uint8_t channel, uint16_t freq, uint8_t waveform, uint16_t a, uint16_t d, uint16_t s, uint16_t r, uint8_t phase);
    extern uint8_t buffer[BUFFER_SIZE];
}