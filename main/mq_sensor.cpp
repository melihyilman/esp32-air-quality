#include "mq_sensor.hpp"

#include <cmath>
#include "esp_adc/adc_cali_scheme.h"

namespace air_quality {

namespace {
constexpr float kSupplyVoltage  = 5.0f;
constexpr float kBaselinePpm    = 420.0f;
constexpr float kCurveExponent  = 1.65f;
}  // namespace

MqSensor::MqSensor(adc_channel_t channel, float r0_kohm, float rl_kohm, float divider_ratio)
    : channel_(channel), r0_kohm_(r0_kohm), rl_kohm_(rl_kohm), divider_ratio_(divider_ratio) {}

MqSensor::~MqSensor() {
    if (cali_handle_) {
        adc_cali_delete_scheme_curve_fitting(cali_handle_);
        cali_handle_ = nullptr;
    }
}

esp_err_t MqSensor::init(adc_oneshot_unit_handle_t adc_handle) {
    if (!adc_handle) return ESP_ERR_INVALID_ARG;
    adc_handle_ = adc_handle;

    adc_oneshot_chan_cfg_t chan_cfg{};
    chan_cfg.atten = ADC_ATTEN_DB_12;
    chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (esp_err_t err = adc_oneshot_config_channel(adc_handle_, channel_, &chan_cfg); err != ESP_OK) {
        return err;
    }

    adc_cali_curve_fitting_config_t cali_cfg{};
    cali_cfg.unit_id = ADC_UNIT_1;
    cali_cfg.chan = channel_;
    cali_cfg.atten = ADC_ATTEN_DB_12;
    cali_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle_) != ESP_OK) {
        cali_handle_ = nullptr;
    }

    return ESP_OK;
}

int MqSensor::readMillivolts(int samples) const {
    if (!adc_handle_ || samples <= 0) return 0;

    int32_t sum_mv = 0;
    int valid = 0;

    for (int i = 0; i < samples; ++i) {
        int raw = 0;
        if (adc_oneshot_read(adc_handle_, channel_, &raw) == ESP_OK) {
            int mv = 0;
            if (cali_handle_ && adc_cali_raw_to_voltage(cali_handle_, raw, &mv) == ESP_OK) {
                sum_mv += mv;
            } else {
                sum_mv += (raw * 3100) / 4095;
            }
            ++valid;
        }
    }

    return valid > 0 ? static_cast<int>(sum_mv / valid) : 0;
}

void MqSensor::calibrateCleanAir(int mv, float target_ppm) {
    const float v_out = std::clamp((static_cast<float>(mv) / 1000.0f) * divider_ratio_, 0.05f, 4.95f);
    const float rs = rl_kohm_ * (kSupplyVoltage - v_out) / v_out;
    const float ratio = std::pow(target_ppm / kBaselinePpm, -1.0f / kCurveExponent);
    if (ratio > 0.01f) {
        r0_kohm_ = rs / ratio;
    }
}

int MqSensor::estimateCo2EqPpm(int mv) const {
    const float v_out = std::clamp((static_cast<float>(mv) / 1000.0f) * divider_ratio_, 0.05f, 4.95f);
    const float rs = rl_kohm_ * (kSupplyVoltage - v_out) / v_out;
    const float ratio = rs / r0_kohm_;
    const float ppm = kBaselinePpm * std::pow(ratio, -kCurveExponent);
    return std::clamp(static_cast<int>(std::lround(ppm)), 380, 5000);
}

}  // namespace air_quality
