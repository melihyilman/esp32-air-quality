#pragma once

#include <cstddef>
#include <cstdint>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

namespace air_quality {

/**
 * @brief Convert 8-bit RGB components into byte-swapped big-endian RGB565 for SPI transmission.
 */
[[nodiscard]] constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) noexcept {
    const uint16_t raw = static_cast<uint16_t>(((r & 0xF8U) << 8U) | ((g & 0xFCU) << 3U) | (b >> 3U));
    return static_cast<uint16_t>((raw >> 8U) | (raw << 8U));
}

/**
 * @brief Zero-dependency SPI driver and 2D gauge primitives for the 240x240 GC9A01 round IPS display.
 */
class Gc9a01Display {
public:
    static constexpr int WIDTH  = 240;
    static constexpr int HEIGHT = 240;
    static constexpr int CENTER = WIDTH / 2;

    struct Pins {
        gpio_num_t scl;
        gpio_num_t sda;
        gpio_num_t cs;
        gpio_num_t dc;
        gpio_num_t rst;
    };

    static constexpr uint16_t COLOR_BG     = rgb565(12, 16, 24);
    static constexpr uint16_t COLOR_TRACK  = rgb565(34, 42, 58);
    static constexpr uint16_t COLOR_WHITE  = rgb565(240, 244, 250);
    static constexpr uint16_t COLOR_MUTED  = rgb565(138, 150, 170);
    static constexpr uint16_t COLOR_GREEN  = rgb565(34, 197, 94);
    static constexpr uint16_t COLOR_YELLOW = rgb565(250, 204, 21);
    static constexpr uint16_t COLOR_ORANGE = rgb565(249, 115, 22);
    static constexpr uint16_t COLOR_RED    = rgb565(239, 68, 68);
    static constexpr uint16_t COLOR_CYAN   = rgb565(56, 189, 248);

    Gc9a01Display() = default;
    ~Gc9a01Display() = default;

    Gc9a01Display(const Gc9a01Display&) = delete;
    Gc9a01Display& operator=(const Gc9a01Display&) = delete;

    [[nodiscard]] esp_err_t init(const Pins& pins,
                                 spi_host_device_t host = SPI2_HOST,
                                 int clock_hz = 40 * 1000 * 1000);

    void fillScreen(uint16_t color);
    void fillRect(int x, int y, int w, int h, uint16_t color);
    void drawPixel(int x, int y, uint16_t color);

    /**
     * @brief Render a 280-degree circular gauge ring (r = 103..115) using scanline span transfers.
     * @param progress_0_to_1 Active fraction of the ring in [0.0, 1.0].
     */
    void drawGaugeRing(float progress_0_to_1, uint16_t active_color);

    /**
     * @brief Render high-resolution chamfered 7-segment numeric digits centered at (cx, y).
     */
    void drawBigNumber(int cx, int y, int value, uint16_t fg, uint16_t bg);

    void drawText(int x, int y, const char* text, uint16_t fg, uint16_t bg, int scale);
    void drawCenteredText(int cx, int y, const char* text, uint16_t fg, uint16_t bg, int scale);

private:
    static constexpr int kChunkLines = 8;

    Pins pins_{};
    spi_device_handle_t spi_{nullptr};
    uint16_t line_buf_[WIDTH * kChunkLines]{};

    void writeCommand(uint8_t cmd);
    void writeData(const uint8_t* data, size_t len);
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void reset();
    void sendInitSequence();
};

}  // namespace air_quality
