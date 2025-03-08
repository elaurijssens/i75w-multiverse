#include "display.hpp"

#include <cstring>

#include "buildinfo.h"

using namespace pimoroni;

namespace display {
    uint8_t buffer[BUFFER_SIZE];        // Actual display buffer
    uint8_t shadow_buffer[BUFFER_SIZE]; // Shadow framebuffer for manipulation
    PicoGraphics_PenRGB888 graphics(WIDTH, HEIGHT, &buffer); // Draw on shadow_buffer
    Hub75 hub75(WIDTH, HEIGHT, nullptr, PANEL_GENERIC, false, Hub75::COLOR_ORDER::RGB);

    const int FONT_HEIGHT = 8;
    const std::string FONT = "bitmap8";

    static int cursor_x = 0;
    static int cursor_y = 0; // Start at the top-left corner

    void __isr dma_complete() {
        hub75.dma_complete();
    }

    void init() {
        hub75.start(dma_complete);
        info(std::to_string(WIDTH) + "x" + std::to_string(HEIGHT) + BOARD_NAME + "\n" + PICO_PLATFORM + "\n" + BUILD_NUMBER);
    }

    void clear() {
        graphics.set_pen(0, 0, 0);
        graphics.clear();
        graphics.set_pen(255, 255, 255);
    }

    void info(std::string_view text) {
        clear();
        graphics.set_font(FONT);
        graphics.text(text, Point(0, 0), WIDTH, 1);
        update();
    }

    void update() {
        // Copy shadow buffer into actual display buffer
        memcpy(buffer, shadow_buffer, BUFFER_SIZE);
        hub75.update(&graphics);
    }

    void scroll_up() {
        // Move all pixels up by FONT_HEIGHT rows in shadow_buffer
        for (int y = 0; y < HEIGHT - FONT_HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int from_index = ((y + FONT_HEIGHT) * WIDTH + x) * 3;  // RGB888 format
                int to_index = (y * WIDTH + x) * 3;
                shadow_buffer[to_index] = shadow_buffer[from_index];      // R
                shadow_buffer[to_index + 1] = shadow_buffer[from_index + 1]; // G
                shadow_buffer[to_index + 2] = shadow_buffer[from_index + 2]; // B
            }
        }

        // Clear only the last FONT_HEIGHT rows in shadow_buffer
        for (int y = HEIGHT - FONT_HEIGHT; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int index = (y * WIDTH + x) * 3;
                shadow_buffer[index] = 0;     // R
                shadow_buffer[index + 1] = 0; // G
                shadow_buffer[index + 2] = 0; // B
            }
        }

        cursor_y -= FONT_HEIGHT;
    }

    void print(std::string_view text, bool append = false) {
        graphics.set_pen(255, 255, 255);
        graphics.set_font(FONT);

        if (!append) text = std::string(text) + '\n'; // Auto append newline if not appending

        for (char c : text) {
            int char_width = graphics.measure_text(std::string(1, c), 1); // Get exact character width

            // Move to a new line when necessary
            if (c == '\n' || cursor_x + char_width >= WIDTH) {
                cursor_x = 0;
                cursor_y += FONT_HEIGHT;

                // Scroll up only if at the bottom
                if (cursor_y + FONT_HEIGHT > HEIGHT) {
                    scroll_up();
                    cursor_y -= FONT_HEIGHT;
                }
                if (c == '\n') continue;
            }

            graphics.text(std::string(1, c), Point(cursor_x, cursor_y), WIDTH, 1);
            cursor_x += char_width; // Move cursor by measured width
        }
        update();
    }
}