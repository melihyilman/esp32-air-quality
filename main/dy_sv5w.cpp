#include "dy_sv5w.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

namespace air_quality {

DySv5wPlayer::DySv5wPlayer(gpio_num_t tx_pin, uart_port_t uart_num)
    : tx_pin_(tx_pin), uart_num_(uart_num) {}

esp_err_t DySv5wPlayer::init(uint8_t initial_volume) {
    uart_config_t uart_cfg{};
    uart_cfg.baud_rate = 9600;
    uart_cfg.data_bits = UART_DATA_8_BITS;
    uart_cfg.parity    = UART_PARITY_DISABLE;
    uart_cfg.stop_bits = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_cfg.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err = uart_driver_install(uart_num_, 256, 0, 0, nullptr, 0);
    if (err != ESP_OK) return err;

    err = uart_param_config(uart_num_, &uart_cfg);
    if (err != ESP_OK) return err;

    err = uart_set_pin(uart_num_, tx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    setVolume(initial_volume);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    setLoopMode(LoopMode::SingleCycle);
    return ESP_OK;
}

void DySv5wPlayer::sendFrame(uint8_t cmd, const uint8_t* data, uint8_t len) {
    uint8_t buf[16];
    if (len > 10) return;

    buf[0] = 0xAA;
    buf[1] = cmd;
    buf[2] = len;

    uint8_t sum = static_cast<uint8_t>(buf[0] + buf[1] + buf[2]);
    for (uint8_t i = 0; i < len; ++i) {
        buf[3 + i] = data[i];
        sum = static_cast<uint8_t>(sum + data[i]);
    }
    buf[3 + len] = sum;

    uart_write_bytes(uart_num_, reinterpret_cast<const char*>(buf), 4 + len);
}

void DySv5wPlayer::setVolume(uint8_t volume_0_to_30) {
    uint8_t v = std::min<uint8_t>(volume_0_to_30, 30);
    sendFrame(0x13, &v, 1);
}

void DySv5wPlayer::setLoopMode(LoopMode mode) {
    uint8_t m = static_cast<uint8_t>(mode);
    sendFrame(0x18, &m, 1);
}

void DySv5wPlayer::playTrack(uint16_t track_number) {
    uint8_t payload[2] = {
        static_cast<uint8_t>((track_number >> 8) & 0xFF),
        static_cast<uint8_t>(track_number & 0xFF),
    };
    sendFrame(0x07, payload, 2);
}

void DySv5wPlayer::stop() {
    sendFrame(0x04, nullptr, 0);
}

}  // namespace air_quality
