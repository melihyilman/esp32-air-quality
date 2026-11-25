#include "gc9a01.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

namespace air_quality {

namespace {

constexpr float kPi         = 3.14159265f;
constexpr int kRingInnerR   = 103;
constexpr int kRingOuterR   = 115;
constexpr float kGapEndDeg  = 220.0f;
constexpr float kGapStartDeg = 140.0f;
constexpr float kSweepDeg   = 280.0f;

struct Glyph {
    char ch;
    uint8_t cols[5];
};

constexpr Glyph kFont[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
    {'!', {0x00, 0x00, 0x5F, 0x00, 0x00}},
    {'%', {0x23, 0x13, 0x08, 0x64, 0x62}},
    {'+', {0x08, 0x08, 0x3E, 0x08, 0x08}},
    {'-', {0x08, 0x08, 0x08, 0x08, 0x08}},
    {'.', {0x00, 0x60, 0x60, 0x00, 0x00}},
    {'/', {0x20, 0x10, 0x08, 0x04, 0x02}},
    {':', {0x00, 0x36, 0x36, 0x00, 0x00}},
    {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
    {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
    {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
    {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
    {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
    {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
    {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
    {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
    {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}},
    {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}},
    {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
    {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
    {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}},
    {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
    {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
    {'J', {0x20, 0x40, 0x41, 0x3F, 0x01}},
    {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}},
    {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
    {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
    {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
    {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}},
    {'Q', {0x3E, 0x41, 0x51, 0x21, 0x5E}},
    {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
    {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
    {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
    {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
    {'V', {0x1F, 0x20, 0x40, 0x20, 0x1F}},
    {'W', {0x3F, 0x40, 0x38, 0x40, 0x3F}},
    {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
    {'Y', {0x07, 0x08, 0x70, 0x08, 0x07}},
    {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}},
    {'a', {0x20, 0x54, 0x54, 0x54, 0x78}},
    {'c', {0x38, 0x44, 0x44, 0x44, 0x20}},
    {'d', {0x38, 0x44, 0x44, 0x48, 0x7F}},
    {'e', {0x38, 0x54, 0x54, 0x54, 0x18}},
    {'g', {0x0C, 0x52, 0x52, 0x52, 0x3E}},
    {'i', {0x00, 0x44, 0x7D, 0x40, 0x00}},
    {'m', {0x7C, 0x04, 0x18, 0x04, 0x78}},
    {'n', {0x7C, 0x08, 0x04, 0x04, 0x78}},
    {'o', {0x38, 0x44, 0x44, 0x44, 0x38}},
    {'p', {0x7C, 0x14, 0x14, 0x14, 0x08}},
    {'r', {0x7C, 0x08, 0x04, 0x04, 0x08}},
    {'s', {0x48, 0x54, 0x54, 0x54, 0x20}},
    {'t', {0x04, 0x3F, 0x44, 0x40, 0x20}},
    {'u', {0x3C, 0x40, 0x40, 0x20, 0x7C}},
    {'w', {0x3C, 0x40, 0x30, 0x40, 0x3C}},
};

const uint8_t* findGlyph(char c) noexcept {
    for (const auto& g : kFont) {
        if (g.ch == c) return g.cols;
    }
    return kFont[0].cols;
}

}  // namespace

esp_err_t Gc9a01Display::init(const Pins& pins, spi_host_device_t host, int clock_hz) {
    pins_ = pins;

    gpio_config_t io_conf{};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << pins_.dc) | (1ULL << pins_.rst);
    if (esp_err_t err = gpio_config(&io_conf); err != ESP_OK) {
        return err;
    }

    spi_bus_config_t bus_cfg{};
    bus_cfg.mosi_io_num = pins_.sda;
    bus_cfg.miso_io_num = -1;
    bus_cfg.sclk_io_num = pins_.scl;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = static_cast<int>( sizeof(line_buf_) );

    if (esp_err_t err = spi_bus_initialize(host, &bus_cfg, SPI_DMA_CH_AUTO); err != ESP_OK) {
        return err;
    }

    spi_device_interface_config_t dev_cfg{};
    dev_cfg.clock_speed_hz = clock_hz;
    dev_cfg.mode = 0;
    dev_cfg.spics_io_num = pins_.cs;
    dev_cfg.queue_size = 7;

    if (esp_err_t err = spi_bus_add_device(host, &dev_cfg, &spi_); err != ESP_OK) {
        return err;
    }

    reset();
    sendInitSequence();
    fillScreen(COLOR_BG);
    return ESP_OK;
}

void Gc9a01Display::reset() {
    gpio_set_level(pins_.rst, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    gpio_set_level(pins_.rst, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
}

void Gc9a01Display::writeCommand(uint8_t cmd) {
    if (!spi_) return;
    gpio_set_level(pins_.dc, 0);
    spi_transaction_t t{};
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_polling_transmit(spi_, &t);
}

void Gc9a01Display::writeData(const uint8_t* data, size_t len) {
    if (!spi_ || !data || len == 0) return;
    gpio_set_level(pins_.dc, 1);
    spi_transaction_t t{};
    t.length = len * 8;
    t.tx_buffer = data;
    spi_device_polling_transmit(spi_, &t);
}

void Gc9a01Display::sendInitSequence() {
    struct InitCmd {
        uint8_t cmd;
        uint8_t len;
        uint8_t data[14];
    };

    static constexpr InitCmd kInitSeq[] = {
        {0xEF, 0, {}},
        {0xEB, 1, {0x14}},
        {0xFE, 0, {}},
        {0xEF, 0, {}},
        {0xEB, 1, {0x14}},
        {0x84, 1, {0x40}},
        {0x85, 1, {0xFF}},
        {0x86, 1, {0xFF}},
        {0x87, 1, {0xFF}},
        {0x88, 1, {0x0A}},
        {0x89, 1, {0x21}},
        {0x8A, 1, {0x00}},
        {0x8B, 1, {0x80}},
        {0x8C, 1, {0x01}},
        {0x8D, 1, {0x01}},
        {0x8E, 1, {0xFF}},
        {0x8F, 1, {0xFF}},
        {0xB6, 2, {0x00, 0x20}},
        {0x36, 1, {0x08}},  // MADCTL: BGR, normal horizontal orientation
        {0x3A, 1, {0x05}},  // COLMOD: 16-bit RGB565
        {0x90, 4, {0x08, 0x08, 0x08, 0x08}},
        {0xBD, 1, {0x06}},
        {0xBC, 1, {0x00}},
        {0xFF, 3, {0x60, 0x01, 0x04}},
        {0xC3, 1, {0x13}},
        {0xC4, 1, {0x13}},
        {0xC9, 1, {0x22}},
        {0xBE, 1, {0x11}},
        {0xE1, 2, {0x10, 0x0E}},
        {0xDF, 3, {0x21, 0x0C, 0x02}},
        {0xF0, 6, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}},
        {0xF1, 6, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}},
        {0xF2, 6, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}},
        {0xF3, 6, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}},
        {0xED, 2, {0x1B, 0x0B}},
        {0xAE, 1, {0x77}},
        {0xCD, 1, {0x63}},
        {0x70, 9, {0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}},
        {0xE8, 1, {0x34}},
        {0x62, 12, {0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}},
        {0x63, 12, {0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}},
        {0x64, 7, {0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}},
        {0x66, 10, {0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}},
        {0x67, 10, {0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}},
        {0x74, 7, {0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}},
        {0x98, 2, {0x3E, 0x07}},
        {0x35, 0, {}},
        {0x21, 0, {}},  // Display inversion ON (required by GC9A01 IPS panel)
    };

    for (const auto& entry : kInitSeq) {
        writeCommand(entry.cmd);
        if (entry.len > 0) {
            writeData(entry.data, entry.len);
        }
    }

    writeCommand(0x11);  // Sleep out
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    writeCommand(0x29);  // Display ON
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

void Gc9a01Display::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    const uint8_t col_data[4] = {
        static_cast<uint8_t>(x0 >> 8U), static_cast<uint8_t>(x0 & 0xFFU),
        static_cast<uint8_t>(x1 >> 8U), static_cast<uint8_t>(x1 & 0xFFU),
    };
    const uint8_t row_data[4] = {
        static_cast<uint8_t>(y0 >> 8U), static_cast<uint8_t>(y0 & 0xFFU),
        static_cast<uint8_t>(y1 >> 8U), static_cast<uint8_t>(y1 & 0xFFU),
    };

    writeCommand(0x2A);
    writeData(col_data, sizeof(col_data));
    writeCommand(0x2B);
    writeData(row_data, sizeof(row_data));
    writeCommand(0x2C);
}

void Gc9a01Display::fillRect(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(WIDTH - 1, x + w - 1);
    const int y1 = std::min(HEIGHT - 1, y + h - 1);
    if (x0 > x1 || y0 > y1) return;

    const int total = (x1 - x0 + 1) * (y1 - y0 + 1);
    constexpr int kChunkPixels = WIDTH * kChunkLines;
    const int fill_count = std::min(total, kChunkPixels);
    std::fill_n(line_buf_, fill_count, color);

    setWindow(static_cast<uint16_t>(x0), static_cast<uint16_t>(y0),
              static_cast<uint16_t>(x1), static_cast<uint16_t>(y1));

    int remaining = total;
    while (remaining > 0) {
        const int batch = std::min(remaining, kChunkPixels);
        writeData(reinterpret_cast<const uint8_t*>(line_buf_),
                  static_cast<size_t>(batch) * sizeof(uint16_t));
        remaining -= batch;
    }
}

void Gc9a01Display::fillScreen(uint16_t color) {
    fillRect(0, 0, WIDTH, HEIGHT, color);
}

void Gc9a01Display::drawPixel(int x, int y, uint16_t color) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    setWindow(static_cast<uint16_t>(x), static_cast<uint16_t>(y),
              static_cast<uint16_t>(x), static_cast<uint16_t>(y));
    writeData(reinterpret_cast<const uint8_t*>(&color), sizeof(color));
}

void Gc9a01Display::drawGaugeRing(float progress_0_to_1, uint16_t active_color) {
    const float clamped = std::clamp(progress_0_to_1, 0.0f, 1.0f);
    const float active_sweep = clamped * kSweepDeg;

    constexpr int kRInner2 = kRingInnerR * kRingInnerR;
    constexpr int kROuter2 = kRingOuterR * kRingOuterR;

    const int y_min = std::max(0, CENTER - kRingOuterR);
    const int y_max = std::min(HEIGHT - 1, CENTER + kRingOuterR);

    for (int y = y_min; y <= y_max; ++y) {
        const int dy = y - CENTER;
        const int dy2 = dy * dy;
        if (dy2 > kROuter2) continue;

        const int max_dx = static_cast<int>(std::sqrt(static_cast<float>(kROuter2 - dy2)));
        const int min_dx = (dy2 < kRInner2)
                               ? static_cast<int>(std::ceil(std::sqrt(static_cast<float>(kRInner2 - dy2))))
                               : 0;

        int segs[2][2] = {
            {CENTER - max_dx, CENTER - min_dx},
            {CENTER + min_dx, CENTER + max_dx},
        };
        const int seg_count = (min_dx > 0) ? 2 : 1;
        if (seg_count == 1) {
            segs[0][0] = CENTER - max_dx;
            segs[0][1] = CENTER + max_dx;
        }

        for (int s = 0; s < seg_count; ++s) {
            const int x_start = std::max(0, segs[s][0]);
            const int x_end   = std::min(WIDTH - 1, segs[s][1]);
            if (x_start > x_end) continue;

            const int len = x_end - x_start + 1;
            for (int x = x_start; x <= x_end; ++x) {
                const int dx = x - CENTER;
                const int d2 = dx * dx + dy2;
                uint16_t px = COLOR_BG;
                if (d2 >= kRInner2 && d2 <= kROuter2) {
                    float deg = std::atan2(static_cast<float>(dx), static_cast<float>(-dy)) * (180.0f / kPi);
                    if (deg < 0.0f) deg += 360.0f;
                    if (deg >= kGapEndDeg || deg <= kGapStartDeg) {
                        const float rel = (deg >= kGapEndDeg) ? (deg - kGapEndDeg) : (deg + kGapStartDeg);
                        px = (rel <= active_sweep) ? active_color : COLOR_TRACK;
                    }
                }
                line_buf_[x - x_start] = px;
            }
            setWindow(static_cast<uint16_t>(x_start), static_cast<uint16_t>(y),
                      static_cast<uint16_t>(x_end), static_cast<uint16_t>(y));
            writeData(reinterpret_cast<const uint8_t*>(line_buf_),
                      static_cast<size_t>(len) * sizeof(uint16_t));
        }
    }
}

void Gc9a01Display::drawBigNumber(int cx, int y, int value, uint16_t fg, uint16_t bg) {
    char digits[8];
    std::snprintf(digits, sizeof(digits), "%d", std::clamp(value, 0, 9999));
    const int n = static_cast<int>(std::strlen(digits));

    constexpr int kDigitW  = 24;
    constexpr int kDigitH  = 40;
    constexpr int kThick   = 5;
    constexpr int kGap     = 5;
    constexpr int kMaxBoxW = 4 * kDigitW + 3 * kGap + 4;

    const int box_x0 = std::clamp(cx - kMaxBoxW / 2, 0, WIDTH - kMaxBoxW);
    const int total_w = n * kDigitW + (n - 1) * kGap;
    const int start_x = cx - total_w / 2;

    // 7-segment bitmask: 0=Top, 1=TopLeft, 2=TopRight, 3=Mid, 4=BotLeft, 5=BotRight, 6=Bot
    static constexpr uint8_t kSegMask[10] = {
        0b1110111,  // 0
        0b0100100,  // 1
        0b1011101,  // 2
        0b1101101,  // 3
        0b0101110,  // 4
        0b1101011,  // 5
        0b1111011,  // 6
        0b0100101,  // 7
        0b1111111,  // 8
        0b1101111,  // 9
    };

    for (int ry = 0; ry < kDigitH; ++ry) {
        if (y + ry < 0 || y + ry >= HEIGHT) continue;
        std::fill_n(line_buf_, kMaxBoxW, bg);

        for (int d = 0; d < n; ++d) {
            const int digit = digits[d] - '0';
            if (digit < 0 || digit > 9) continue;
            const uint8_t mask = kSegMask[digit];
            const int dx0 = (start_x - box_x0) + d * (kDigitW + kGap);

            auto fillSpan = [&](int x0, int x1) {
                const int lo = std::max(0, x0);
                const int hi = std::min(kMaxBoxW - 1, x1);
                for (int x = lo; x <= hi; ++x) {
                    line_buf_[x] = fg;
                }
            };

            if ((mask & (1U << 0U)) && ry < kThick) {
                const int inset = (ry == 0) ? 2 : 1;
                fillSpan(dx0 + inset, dx0 + kDigitW - 1 - inset);
            }
            if ((mask & (1U << 3U)) && ry >= (kDigitH / 2 - kThick / 2) &&
                ry < (kDigitH / 2 + (kThick + 1) / 2)) {
                fillSpan(dx0 + 2, dx0 + kDigitW - 3);
            }
            if ((mask & (1U << 6U)) && ry >= kDigitH - kThick) {
                const int inset = (ry == kDigitH - 1) ? 2 : 1;
                fillSpan(dx0 + inset, dx0 + kDigitW - 1 - inset);
            }
            if ((mask & (1U << 1U)) && ry >= 2 && ry <= kDigitH / 2) {
                fillSpan(dx0, dx0 + kThick - 1);
            }
            if ((mask & (1U << 2U)) && ry >= 2 && ry <= kDigitH / 2) {
                fillSpan(dx0 + kDigitW - kThick, dx0 + kDigitW - 1);
            }
            if ((mask & (1U << 4U)) && ry >= kDigitH / 2 && ry <= kDigitH - 3) {
                fillSpan(dx0, dx0 + kThick - 1);
            }
            if ((mask & (1U << 5U)) && ry >= kDigitH / 2 && ry <= kDigitH - 3) {
                fillSpan(dx0 + kDigitW - kThick, dx0 + kDigitW - 1);
            }
        }

        setWindow(static_cast<uint16_t>(box_x0), static_cast<uint16_t>(y + ry),
                  static_cast<uint16_t>(box_x0 + kMaxBoxW - 1), static_cast<uint16_t>(y + ry));
        writeData(reinterpret_cast<const uint8_t*>(line_buf_),
                  static_cast<size_t>(kMaxBoxW) * sizeof(uint16_t));
    }
}

void Gc9a01Display::drawText(int x, int y, const char* text, uint16_t fg, uint16_t bg, int scale) {
    if (!text || scale <= 0) return;
    const int safe_scale = std::min(scale, 4);
    const int char_w = 5 * safe_scale;
    const int char_h = 7 * safe_scale;
    int cursor_x = x;

    for (const char* p = text; *p != '\0'; ++p) {
        const uint8_t* cols = findGlyph(*p);

        for (int row = 0; row < 7; ++row) {
            for (int sy = 0; sy < safe_scale; ++sy) {
                const int py = row * safe_scale + sy;
                for (int col = 0; col < 5; ++col) {
                    const bool on = (cols[col] & (1U << row)) != 0;
                    const uint16_t px_color = on ? fg : bg;
                    for (int sx = 0; sx < safe_scale; ++sx) {
                        line_buf_[py * char_w + col * safe_scale + sx] = px_color;
                    }
                }
            }
        }

        if (cursor_x >= 0 && cursor_x + char_w <= WIDTH && y >= 0 && y + char_h <= HEIGHT) {
            setWindow(static_cast<uint16_t>(cursor_x), static_cast<uint16_t>(y),
                      static_cast<uint16_t>(cursor_x + char_w - 1),
                      static_cast<uint16_t>(y + char_h - 1));
            writeData(reinterpret_cast<const uint8_t*>(line_buf_),
                      static_cast<size_t>(char_w * char_h) * sizeof(uint16_t));
        }
        fillRect(cursor_x + char_w, y, safe_scale, char_h, bg);
        cursor_x += char_w + safe_scale;
    }
}

void Gc9a01Display::drawCenteredText(int cx, int y, const char* text, uint16_t fg, uint16_t bg, int scale) {
    if (!text || scale <= 0) return;
    const size_t len = std::strlen(text);
    if (len == 0) return;
    const int total_w = static_cast<int>(len) * (6 * scale) - scale;
    drawText(cx - total_w / 2, y, text, fg, bg, scale);
}

}  // namespace air_quality
