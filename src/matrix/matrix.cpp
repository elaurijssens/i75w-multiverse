#include "matrix.hpp"
#include "buildinfo.h"
#include <deque>
#include <cstring>
#include "config_storage.hpp"
#include <unordered_map>

using namespace pimoroni;

namespace matrix {
    uint8_t buffer[BUFFER_SIZE];
    PicoGraphics_PenRGB888 graphics(WIDTH, HEIGHT, &buffer);
    Hub75* hub75 = nullptr;

    const int FONT_HEIGHT = 8;
    const std::string FONT = "bitmap8";
    const int MAX_LINES = HEIGHT / FONT_HEIGHT;

    static std::deque<char> text_buffer; // Store characters dynamically

    void __isr dma_complete() {
        if (hub75) hub75->dma_complete();
    }

    void init(KVStore& kvStore) {  // ✅ Pass `kvStore` to `init`
        if (!hub75) {
            // ✅ Initialize `Hub75` dynamically using kvStore
            static const std::unordered_map<std::string, Hub75::COLOR_ORDER> color_order_map = {
                {"RGB", Hub75::COLOR_ORDER::RGB},
                {"RBG", Hub75::COLOR_ORDER::RBG},
                {"GRB", Hub75::COLOR_ORDER::GRB},
                {"GBR", Hub75::COLOR_ORDER::GBR},
                {"BRG", Hub75::COLOR_ORDER::BRG},
                {"BGR", Hub75::COLOR_ORDER::BGR}
            };

            std::string color_order_str = kvStore.getParam("color_order");  // Read from storage
            DEBUG_PRINT("Color order in kv: " + color_order_str );
            Hub75::COLOR_ORDER color_order = Hub75::COLOR_ORDER::RGB;  // Default


            // ✅ Trim spaces & newlines
            color_order_str.erase(0, color_order_str.find_first_not_of(" \t\n\r"));
            color_order_str.erase(color_order_str.find_last_not_of(" \t\n\r") + 1);

            // ✅ Convert to uppercase
            std::transform(color_order_str.begin(), color_order_str.end(), color_order_str.begin(), ::toupper);

            // ✅ Debug prints
            DEBUG_PRINT("Raw color_order_str: [" + color_order_str + "]");

            // ✅ Force comparison check
            for (const auto& pair : color_order_map) {
                if (pair.first == color_order_str) {
                    DEBUG_PRINT("✅ Matched key: " + pair.first);
                }
            }

            if (color_order_map.count(color_order_str)) {
                color_order = color_order_map.at(color_order_str);
                DEBUG_PRINT("Mapped color order to: " + color_order_str);
            } else {
                DEBUG_PRINT("Color order not found in map, using default: " + color_order_str);
            }

            DEBUG_PRINT("Color order: " + std::to_string(static_cast<int>(color_order)) );



            hub75 = new Hub75(WIDTH, HEIGHT, nullptr, PANEL_GENERIC, false, color_order);
        }

        hub75->start(dma_complete);
        print(std::to_string(WIDTH) + "x" + std::to_string(HEIGHT) + " - " + BOARD_NAME + "\n" + PICO_PLATFORM + "\n" + BUILD_NUMBER);
    }

    void clear() {
        graphics.set_pen(0, 0, 0);
        graphics.clear();
        graphics.set_pen(255, 255, 255);
    }


    void update() {
        if (hub75) hub75->update(&graphics);
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

    void clearscreen() {
        text_buffer.clear();
        redraw();
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
