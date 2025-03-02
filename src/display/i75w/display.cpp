#include "display.hpp"
#include "buildinfo.h"

using namespace pimoroni;

namespace display {
    uint8_t buffer[BUFFER_SIZE];
    PicoGraphics_PenRGB888 graphics(WIDTH, HEIGHT, &buffer);
    Hub75 hub75(WIDTH, HEIGHT, nullptr, PANEL_GENERIC, false, Hub75::COLOR_ORDER::RGB);

    void __isr dma_complete() {
        hub75.dma_complete();
    }

    void init() {
        hub75.start(dma_complete);

        info(std::to_string(WIDTH)+"x"+std::to_string(HEIGHT)+BOARD_NAME+"\n"+PICO_PLATFORM+"\n"+BUILD_NUMBER);
    }

    void info(std::string_view text) {
        graphics.set_pen(0, 0, 0);
        graphics.clear();
        graphics.set_pen(255, 255, 255);
        graphics.set_font("bitmap8");
        graphics.text(text, Point(0, 0), WIDTH, 1);
        update();
    }

    void update() {
        hub75.update(&graphics);
    }

}