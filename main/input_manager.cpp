#include "input_manager.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

namespace {

constexpr const char *kTag = "input_manager";
constexpr adc_channel_t kKeyChannel = ADC_CHANNEL_0;
adc_oneshot_unit_handle_t s_adc_handle;

InputKey decodeKey(int value)
{
    if (value < 260) return InputKey::Key5;
    if (value < 825) return InputKey::Key4;
    if (value < 1497) return InputKey::Key3;
    if (value < 2455) return InputKey::Key2;
    if (value < 3569) return InputKey::Key1;
    return InputKey::None;
}

}  // namespace

esp_err_t InputManager::init()
{
    adc_oneshot_unit_init_cfg_t init_config = {};
    init_config.unit_id = ADC_UNIT_1;
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_config, &s_adc_handle), kTag, "adc unit");

    adc_oneshot_chan_cfg_t config = {};
    config.atten = ADC_ATTEN_DB_12;
    config.bitwidth = ADC_BITWIDTH_12;
    return adc_oneshot_config_channel(s_adc_handle, kKeyChannel, &config);
}

AppEvent InputManager::pollEvent()
{
    int raw = 0;
    if (adc_oneshot_read(s_adc_handle, kKeyChannel, &raw) != ESP_OK) {
        return {};
    }

    const InputKey current_key = decodeKey(raw);
    if (current_key == last_key_) {
        return {};
    }

    last_key_ = current_key;
    if (current_key == InputKey::None) {
        return {};
    }

    ESP_LOGI(kTag, "adc=%d key=%d", raw, static_cast<int>(current_key));
    return AppEvent{
        .type = AppEventType::KeyPressed,
        .key = current_key,
    };
}
