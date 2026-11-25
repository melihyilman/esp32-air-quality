#pragma once

#include <algorithm>
#include <cstdint>
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

namespace air_quality {

enum class AirQualityLevel : uint8_t {
    Good,
    Moderate,
    Poor,
    Hazard,
};

/**
 * @brief RAII ADC driver for MQ-series analog gas sensors (MQ-135 / MQ-7).
 */
class MqSensor {
public:
    MqSensor(adc_channel_t channel, float r0_kohm, float rl_kohm = 10.0f, float divider_ratio = 2.0f);
    ~MqSensor();

    MqSensor(const MqSensor&) = delete;
    MqSensor& operator=(const MqSensor&) = delete;

    [[nodiscard]] esp_err_t init(adc_oneshot_unit_handle_t adc_handle);

    /**
     * @brief Sample the ADC multiple times and return averaged millivolts.
     */
    [[nodiscard]] int readMillivolts(int samples = 16) const;

    /**
     * @brief Calibrate R0 so the given clean-air millivolt reading corresponds to target_ppm (~420 ppm).
     */
    void calibrateCleanAir(int mv, float target_ppm = 420.0f);

    /**
     * @brief Estimate CO2-equivalent PPM from measured ADC millivolts.
     */
    [[nodiscard]] int estimateCo2EqPpm(int mv) const;

    /**
     * @brief Compute a 0..100% Air Quality Index score from estimated CO2 PPM.
     */
    [[nodiscard]] static constexpr int airQualityPercent(int ppm) noexcept {
        if (ppm <= 420) return 100;
        if (ppm >= 2500) return 0;
        return std::clamp(100 - ((ppm - 420) * 100) / (2500 - 420), 0, 100);
    }

    /**
     * @brief Classify air quality tier from estimated PPM.
     */
    [[nodiscard]] static constexpr AirQualityLevel classify(int ppm) noexcept {
        if (ppm < 800) return AirQualityLevel::Good;
        if (ppm < 1200) return AirQualityLevel::Moderate;
        if (ppm < 2000) return AirQualityLevel::Poor;
        return AirQualityLevel::Hazard;
    }

    [[nodiscard]] static constexpr const char* levelLabel(AirQualityLevel level) noexcept {
        switch (level) {
            case AirQualityLevel::Good:     return "GOOD";
            case AirQualityLevel::Moderate: return "MODERATE";
            case AirQualityLevel::Poor:     return "POOR";
            case AirQualityLevel::Hazard:   return "HAZARD!";
        }
        return "UNKNOWN";
    }

private:
    adc_channel_t channel_;
    float r0_kohm_;
    float rl_kohm_;
    float divider_ratio_;
    adc_oneshot_unit_handle_t adc_handle_{nullptr};
    adc_cali_handle_t cali_handle_{nullptr};
};

}  // namespace air_quality
