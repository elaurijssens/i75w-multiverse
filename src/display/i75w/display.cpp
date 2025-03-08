#include "display.hpp"
#include "buildinfo.h"
#include <deque>
#include <string>

using namespace pimoroni;

namespace display {
    uint8_t buffer[BUFFER_SIZE];
    PicoGraphics_PenRGB888 graphics(WIDTH, HEIGHT, &buffer);
    Hub75 hub75(WIDTH, HEIGHT, nullptr, PANEL_GENERIC, false, Hub75::COLOR_ORDER::RGB);

    const int FONT_HEIGHT = 8;
    const std::string FONT = "bitmap8";
    const int MAX_LINES = HEIGHT / FONT_HEIGHT;

    static std::deque<char> text_buffer; // Store characters dynamically

    void __isr dma_complete() {
        hub75.dma_complete();
    }

    void init() {
        hub75.start(dma_complete);
        print(std::to_string(WIDTH) + "x" + std::to_string(HEIGHT) + " - " + BOARD_NAME + "\n" + PICO_PLATFORM + "\n" + BUILD_NUMBER);
    }

    void clear() {
        graphics.set_pen(0, 0, 0);
        graphics.clear();
        graphics.set_pen(255, 255, 255);
    }

    void update() {
        hub75.update(&graphics);
    }

    void scroll() {
        // Remove characters up to and including the first '\n' if buffer exceeds max lines
        int line_count = 0;
        for (char c : text_buffer) {
            if (c == '\n') {
                line_count++;
            }
        }

        if (line_count >= MAX_LINES) {
            while (!text_buffer.empty()) {
                char c = text_buffer.front();
                text_buffer.pop_front();
                if (c == '\n') break;
            }
        }
    }

    void redraw() {
        std::string text_output(text_buffer.begin(), text_buffer.end());
        info(text_output);
    }

    void info(std::string text) {
        clear();
        graphics.set_font(FONT);

        int line_number = 0;
        size_t start = 0;
        size_t end = 0;
        while ((end = text.find('\n', start)) != std::string::npos) {
            graphics.text(text.substr(start, end - start), Point(0, FONT_HEIGHT * line_number), WIDTH, 1, 0, 1, false);
            start = end + 1;
            line_number++;
        }


        // Print last line (or if no newline was found)
        if (start < text.size()) {
            graphics.text(text.substr(start), Point(0, FONT_HEIGHT * line_number), WIDTH, 1, 0, 1, false);
        }

        update();
    }

    void print(std::string text, bool append) {

        std::string temp_line = "";

        graphics.set_pen(255, 255, 255);
        graphics.set_font(FONT);

        if (!append) text = text + "\n"; // Ensure new prints start on a new line

        for (char c : text) {
            // Measure width of current line

            int current_width = graphics.measure_text(temp_line + c, 1, 1, false);
            int char_width = graphics.measure_text(std::string(1, c), 1, 1, false);

            if (current_width + char_width >= WIDTH) {
                text_buffer.push_back('\n'); // Insert newline if line is full
                temp_line = "";
                redraw();
            }

            scroll();

            text_buffer.push_back(c);
        }
        redraw();
    }
}
