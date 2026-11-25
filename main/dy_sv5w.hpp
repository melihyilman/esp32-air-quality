#pragma once

#include <cstdint>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "hal/uart_types.h"

namespace air_quality {

/**
 * @brief UART driver for the DY-SV5W MP3 voice playback module.
 *
 * Requires DY-SV5W DIP switches set to UART mode:
 *   Switch 1 = OFF (0), Switch 2 = OFF (0), Switch 3 = ON (1)
 */
class DySv5wPlayer {
public:
    enum class LoopMode : uint8_t {
        FullCycle   = 0x00,
        SingleCycle = 0x01,
        SingleStop  = 0x02,
    };

    explicit DySv5wPlayer(gpio_num_t tx_pin = GPIO_NUM_3, uart_port_t uart_num = UART_NUM_1);
    ~DySv5wPlayer() = default;

    DySv5wPlayer(const DySv5wPlayer&) = delete;
    DySv5wPlayer& operator=(const DySv5wPlayer&) = delete;

    [[nodiscard]] esp_err_t init(uint8_t initial_volume = 22);

    void setVolume(uint8_t volume_0_to_30);
    void setLoopMode(LoopMode mode);
    void playTrack(uint16_t track_number);
    void stop();

private:
    gpio_num_t tx_pin_;
    uart_port_t uart_num_;

    void sendFrame(uint8_t cmd, const uint8_t* data, uint8_t len);
};

}  // namespace air_quality
