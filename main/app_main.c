#include <inttypes.h>

#include "esp_log.h"
#include "pomodoro_core.h"
#include "pomodoro_platform.h"

static const char *TAG = "pomodoro";

void app_main(void)
{
    pomodoro_model_t model;
    pomodoro_settings_t settings = pomodoro_default_settings();

    if (pomodoro_init(&model, &settings) != POMODORO_OK) {
        ESP_LOGE(TAG, "Default settings are invalid");
        return;
    }

    const pomodoro_platform_t *platform = pomodoro_platform_get();
    if (platform == NULL) {
        ESP_LOGW(TAG, "Hardware platform is not installed; running core only");
        return;
    }

    ESP_LOGI(TAG, "Pomodoro ready at epoch=%" PRIu64,
             platform->clock.epoch_seconds(platform->context));
}
