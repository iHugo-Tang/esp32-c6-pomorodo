#include "esp_log.h"
#include "pomodoro_platform.h"

static const char *TAG = "pomodoro";

void app_main(void)
{
    esp_err_t result = pomodoro_platform_start();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Device startup failed: %s", esp_err_to_name(result));
    }
}
