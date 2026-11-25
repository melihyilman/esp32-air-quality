#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

#include "gc9a01.hpp"
#include "mq_sensor.hpp"

namespace {

constexpr const char* kTag = "air_quality";

constexpr gpio_num_t kOnboardBlueLed = GPIO_NUM_8;  // Active-low onboard blue LED
constexpr adc_channel_t kMq135Channel = ADC_CHANNEL_0;  // GPIO0 via 10k+10k divider

constexpr air_quality::Gc9a01Display::Pins kDisplayPins{
    .scl = GPIO_NUM_4,
    .sda = GPIO_NUM_6,
    .cs  = GPIO_NUM_7,
    .dc  = GPIO_NUM_5,
    .rst = GPIO_NUM_10,
};

constexpr int kWarmupSeconds = 10;
constexpr float kDefaultR0Kohm = 74.7f;
constexpr float kLoadResistorKohm = 10.0f;
constexpr float kDividerRatio = 2.0f;

// 240x240 Circular Screen Layout Coordinates
struct UiLayout {
    static constexpr int kCenterX       = 120;
    static constexpr int kTitleY        = 55;
    static constexpr int kWarmupTextY   = 98;
    static constexpr int kWarmupMvY     = 132;
    static constexpr int kBigNumberY    = 78;
    static constexpr int kUnitLabelY    = 124;
    static constexpr int kStatusBandX   = 42;
    static constexpr int kStatusBandY   = 140;
    static constexpr int kStatusBandW   = 156;
    static constexpr int kStatusBandH   = 18;
    static constexpr int kStatusTextY   = 142;
    static constexpr int kBottomMetricY = 172;
};

[[nodiscard]] constexpr uint16_t colorForLevel(air_quality::AirQualityLevel level) noexcept {
    using air_quality::AirQualityLevel;
    using air_quality::Gc9a01Display;
    switch (level) {
        case AirQualityLevel::Good:     return Gc9a01Display::COLOR_GREEN;
        case AirQualityLevel::Moderate: return Gc9a01Display::COLOR_YELLOW;
        case AirQualityLevel::Poor:     return Gc9a01Display::COLOR_ORANGE;
        case AirQualityLevel::Hazard:   return Gc9a01Display::COLOR_RED;
    }
    return Gc9a01Display::COLOR_WHITE;
}

void drawStaticChrome(air_quality::Gc9a01Display& lcd) {
    using air_quality::Gc9a01Display;
    lcd.fillScreen(Gc9a01Display::COLOR_BG);
    lcd.drawGaugeRing(0.0f, Gc9a01Display::COLOR_CYAN);
    lcd.drawCenteredText(UiLayout::kCenterX, UiLayout::kTitleY, "AIR QUALITY",
                         Gc9a01Display::COLOR_MUTED, Gc9a01Display::COLOR_BG, 2);
}

void renderWarmup(air_quality::Gc9a01Display& lcd, int remaining_sec, int mq135_mv) {
    using air_quality::Gc9a01Display;
    const float progress = static_cast<float>(kWarmupSeconds - remaining_sec) /
                           static_cast<float>(kWarmupSeconds);
    lcd.drawGaugeRing(progress, Gc9a01Display::COLOR_CYAN);

    char buf[24];
    std::snprintf(buf, sizeof(buf), "WARMUP %2ds", remaining_sec);
    lcd.drawCenteredText(UiLayout::kCenterX, UiLayout::kWarmupTextY, buf,
                         Gc9a01Display::COLOR_WHITE, Gc9a01Display::COLOR_BG, 2);

    std::snprintf(buf, sizeof(buf), "ADC: %4dmV", mq135_mv);
    lcd.drawCenteredText(UiLayout::kCenterX, UiLayout::kWarmupMvY, buf,
                         Gc9a01Display::COLOR_MUTED, Gc9a01Display::COLOR_BG, 2);
}

void renderReading(air_quality::Gc9a01Display& lcd, int ppm, int mq135_mv) {
    using air_quality::Gc9a01Display;
    using air_quality::MqSensor;

    const auto level = MqSensor::classify(ppm);
    const uint16_t ring_color = colorForLevel(level);

    const float norm = std::clamp((static_cast<float>(ppm) - 400.0f) / 2100.0f, 0.05f, 1.0f);
    lcd.drawGaugeRing(norm, ring_color);

    lcd.drawBigNumber(UiLayout::kCenterX, UiLayout::kBigNumberY, ppm,
                      Gc9a01Display::COLOR_WHITE, Gc9a01Display::COLOR_BG);
    lcd.drawCenteredText(UiLayout::kCenterX, UiLayout::kUnitLabelY, "CO2 PPM",
                         Gc9a01Display::COLOR_MUTED, Gc9a01Display::COLOR_BG, 1);

    lcd.fillRect(UiLayout::kStatusBandX, UiLayout::kStatusBandY,
                 UiLayout::kStatusBandW, UiLayout::kStatusBandH, Gc9a01Display::COLOR_BG);
    lcd.drawCenteredText(UiLayout::kCenterX, UiLayout::kStatusTextY, MqSensor::levelLabel(level),
                         ring_color, Gc9a01Display::COLOR_BG, 2);

    const int aq_pct = MqSensor::airQualityPercent(ppm);
    char mv_buf[28];
    std::snprintf(mv_buf, sizeof(mv_buf), "AIR:%3d%%  %3dmV", aq_pct, mq135_mv);
    lcd.drawCenteredText(UiLayout::kCenterX, UiLayout::kBottomMetricY, mv_buf,
                         Gc9a01Display::COLOR_MUTED, Gc9a01Display::COLOR_BG, 1);
}

}  // namespace

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "Starting Air Quality Monitor (ESP32-C3 Native C++20)");

    gpio_config_t led_cfg{};
    led_cfg.mode = GPIO_MODE_OUTPUT;
    led_cfg.pin_bit_mask = (1ULL << kOnboardBlueLed);
    ESP_ERROR_CHECK(gpio_config(&led_cfg));
    bool led_state = false;

    air_quality::Gc9a01Display lcd;
    ESP_ERROR_CHECK(lcd.init(kDisplayPins));
    drawStaticChrome(lcd);

    adc_oneshot_unit_handle_t adc_handle = nullptr;
    adc_oneshot_unit_init_cfg_t unit_cfg{};
    unit_cfg.unit_id = ADC_UNIT_1;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    air_quality::MqSensor mq135(kMq135Channel, kDefaultR0Kohm, kLoadResistorKohm, kDividerRatio);
    ESP_ERROR_CHECK(mq135.init(adc_handle));

    float smoothed_mv = static_cast<float>(mq135.readMillivolts(16));

    for (int sec = kWarmupSeconds; sec > 0; --sec) {
        led_state = !led_state;
        gpio_set_level(kOnboardBlueLed, led_state ? 0 : 1);
        const int raw_mv = mq135.readMillivolts(16);
        smoothed_mv = 0.75f * smoothed_mv + 0.25f * static_cast<float>(raw_mv);
        const int mv = static_cast<int>(std::lround(smoothed_mv));
        renderWarmup(lcd, sec, mv);
        ESP_LOGI(kTag, "Warmup %2ds | IO0=%dmV", sec, mv);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    const int baseline_mv = static_cast<int>(std::lround(smoothed_mv));
    if (baseline_mv > 60 && baseline_mv < 2200) {
        mq135.calibrateCleanAir(baseline_mv, 420.0f);
        ESP_LOGI(kTag, "Calibrated clean-air baseline at IO0=%dmV -> 420 ppm", baseline_mv);
    }

    lcd.fillRect(42, 76, 156, 108, air_quality::Gc9a01Display::COLOR_BG);

    while (true) {
        led_state = !led_state;
        gpio_set_level(kOnboardBlueLed, led_state ? 0 : 1);
        const int raw_mv = mq135.readMillivolts(16);
        smoothed_mv = 0.75f * smoothed_mv + 0.25f * static_cast<float>(raw_mv);
        const int mq135_mv = static_cast<int>(std::lround(smoothed_mv));
        const int ppm = mq135.estimateCo2EqPpm(mq135_mv);
        const auto current_level = air_quality::MqSensor::classify(ppm);

        renderReading(lcd, ppm, mq135_mv);
        ESP_LOGI(kTag, "RAW=%dmV | SMOOTH=%dmV (AO~%dmV) | CO2=%d ppm (%s) | AIR=%d%%",
                 raw_mv, mq135_mv, mq135_mv * 2, ppm,
                 air_quality::MqSensor::levelLabel(current_level),
                 air_quality::MqSensor::airQualityPercent(ppm));

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}
