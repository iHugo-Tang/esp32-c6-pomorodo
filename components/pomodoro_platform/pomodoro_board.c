#include "pomodoro_board.h"

#include <stddef.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"
#include "esp_lcd_touch_cst9217.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#define BOARD_I2C_SDA GPIO_NUM_8
#define BOARD_I2C_SCL GPIO_NUM_7
#define LCD_CS GPIO_NUM_15
#define LCD_PCLK GPIO_NUM_0
#define LCD_D0 GPIO_NUM_1
#define LCD_D1 GPIO_NUM_2
#define LCD_D2 GPIO_NUM_3
#define LCD_D3 GPIO_NUM_4
#define TOUCH_RESET GPIO_NUM_11
#define TOUCH_INTERRUPT GPIO_NUM_5
#define KEY_VOLUME_UP GPIO_NUM_18
#define BOOT_VOLUME_DOWN GPIO_NUM_9
#define AUDIO_MCLK GPIO_NUM_19
#define AUDIO_BCLK GPIO_NUM_20
#define AUDIO_WS GPIO_NUM_22
#define AUDIO_DOUT GPIO_NUM_23
#define AUDIO_DIN GPIO_NUM_21
#define AUDIO_SAMPLE_RATE 16000U
#define DISPLAY_WIDTH 480U
#define DISPLAY_HEIGHT 480U
#define DRAW_BUFFER_ROWS 20U
#define I2C_TIMEOUT_MS 250
#define AXP2101_ADDRESS 0x34U
#define PCF85063_ADDRESS 0x51U
#define PCF85063_SECONDS_REGISTER 0x04U
#define AXP2101_STATUS1_REGISTER 0x00U
#define AXP2101_STATUS2_REGISTER 0x01U
#define AXP2101_COMMON_CONFIG_REGISTER 0x10U
#define AXP2101_BATTERY_PERCENT_REGISTER 0xA4U
#define AXP2101_INTERRUPT_ENABLE2_REGISTER 0x41U
#define AXP2101_INTERRUPT_STATUS2_REGISTER 0x49U
#define AXP2101_POWER_SHORT_PRESS_BIT 0x08U
#define AXP2101_POWER_LONG_PRESS_BIT 0x04U

typedef struct {
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_dev_handle_t pmic;
    i2c_master_dev_handle_t rtc;
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t panel;
    esp_lcd_touch_handle_t touch;
    lv_display_t *display;
    lv_indev_t *touch_input;
} board_context_t;

static const char *TAG = "pomodoro_board";
static board_context_t board;
static uint64_t last_valid_utc_epoch_seconds;
static int64_t last_valid_monotonic_microseconds;
static bool has_last_valid_time;
static i2s_chan_handle_t audio_tx_channel;
static esp_codec_dev_handle_t audio_device;
static bool audio_busy;
static bool queued_focus_tone;
static uint8_t queued_tone_volume;

static const sh8601_lcd_init_cmd_t lcd_init_commands[] = {
    {0x11, NULL, 0, 120},
    {0xC4, (const uint8_t[]){0x80}, 1, 0},
    {0x44, (const uint8_t[]){0x01, 0xD1}, 2, 0},
    {0x35, (const uint8_t[]){0x00}, 1, 0},
    {0x36, (const uint8_t[]){0xA0}, 1, 0},
    {0x53, (const uint8_t[]){0x20}, 1, 10},
    {0x63, (const uint8_t[]){0xFF}, 1, 10},
    {0x51, (const uint8_t[]){0x00}, 1, 10},
    {0x2A, (const uint8_t[]){0x00, 0x16, 0x01, 0xAF}, 4, 0},
    {0x2B, (const uint8_t[]){0x00, 0x00, 0x01, 0xF5}, 4, 0},
    {0x29, NULL, 0, 10},
    {0x51, (const uint8_t[]){0xFF}, 1, 0},
};

static esp_err_t add_i2c_device(uint16_t address,
                                i2c_master_dev_handle_t *handle)
{
    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 400000,
    };
    return i2c_master_bus_add_device(board.i2c_bus, &config, handle);
}

static esp_err_t read_register(i2c_master_dev_handle_t handle, uint8_t reg,
                               uint8_t *data, size_t size)
{
    return i2c_master_transmit_receive(handle, &reg, 1, data, size,
                                       I2C_TIMEOUT_MS);
}

static esp_err_t write_register(i2c_master_dev_handle_t handle, uint8_t reg,
                                uint8_t value)
{
    uint8_t command[] = {reg, value};
    return i2c_master_transmit(handle, command, sizeof(command),
                               I2C_TIMEOUT_MS);
}

static esp_err_t write_registers(i2c_master_dev_handle_t handle, uint8_t reg,
                                 const uint8_t *data, size_t size)
{
    uint8_t command[8];
    if (data == NULL || size + 1U > sizeof(command)) {
        return ESP_ERR_INVALID_ARG;
    }
    command[0] = reg;
    memcpy(&command[1], data, size);
    return i2c_master_transmit(handle, command, size + 1U, I2C_TIMEOUT_MS);
}

static uint8_t from_bcd(uint8_t value)
{
    return (uint8_t) (((value >> 4U) * 10U) + (value & 0x0FU));
}

static uint8_t to_bcd(uint8_t value)
{
    return (uint8_t) (((value / 10U) << 4U) | (value % 10U));
}

static void use_monotonic_fallback(pomodoro_application_time_t *time)
{
    int64_t now = esp_timer_get_time();
    uint64_t elapsed_seconds = has_last_valid_time &&
        now >= last_valid_monotonic_microseconds
        ? (uint64_t) (now - last_valid_monotonic_microseconds) / 1000000U
        : (uint64_t) now / 1000000U;
    *time = (pomodoro_application_time_t) {
        .epoch_seconds = has_last_valid_time
            ? last_valid_utc_epoch_seconds + elapsed_seconds
            : elapsed_seconds,
        .rtc_is_valid = false,
    };
}

static bool bcd_is_valid(uint8_t value)
{
    return (value & 0x0FU) <= 9U && ((value >> 4U) & 0x0FU) <= 9U;
}

static bool is_leap_year(uint16_t year)
{
    return (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;
}

static bool date_is_valid(uint16_t year, uint8_t month, uint8_t day,
                          uint8_t hour, uint8_t minute, uint8_t second)
{
    static const uint8_t days_per_month[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    if (year < 2020U || year > 2069U || month < 1U || month > 12U ||
        hour > 23U || minute > 59U || second > 59U) {
        return false;
    }
    uint8_t limit = days_per_month[month - 1U];
    if (month == 2U && is_leap_year(year)) {
        limit = 29U;
    }
    return day >= 1U && day <= limit;
}

bool pomodoro_board_read_time(pomodoro_application_time_t *time)
{
    uint8_t raw[7];
    if (time == NULL || board.rtc == NULL) {
        return false;
    }
    if (read_register(board.rtc, PCF85063_SECONDS_REGISTER, raw,
                      sizeof(raw)) != ESP_OK) {
        ESP_LOGE(TAG, "RTC read failed");
        use_monotonic_fallback(time);
        return true;
    }

    uint8_t second_bcd = raw[0] & 0x7FU;
    uint8_t minute_bcd = raw[1] & 0x7FU;
    uint8_t hour_bcd = raw[2] & 0x3FU;
    uint8_t day_bcd = raw[3] & 0x3FU;
    uint8_t month_bcd = raw[5] & 0x1FU;
    bool bcd_valid = bcd_is_valid(second_bcd) && bcd_is_valid(minute_bcd) &&
        bcd_is_valid(hour_bcd) && bcd_is_valid(day_bcd) &&
        bcd_is_valid(month_bcd) && bcd_is_valid(raw[6]);
    bool oscillator_stopped = (raw[0] & 0x80U) != 0U;
    uint8_t second = from_bcd(second_bcd);
    uint8_t minute = from_bcd(minute_bcd);
    uint8_t hour = from_bcd(hour_bcd);
    uint8_t day = from_bcd(day_bcd);
    uint8_t month = from_bcd(month_bcd);
    /* Waveshare's board examples store PCF85063 years relative to 1970. */
    uint16_t year = (uint16_t) (1970U + from_bcd(raw[6]));
    bool valid = bcd_valid && !oscillator_stopped &&
        date_is_valid(year, month, day, hour, minute, second);
    if (!valid) {
        ESP_LOGE(TAG, "RTC invalid (OS=%d %04u-%02u-%02u %02u:%02u:%02u)",
                 oscillator_stopped, year, month, day, hour, minute, second);
        use_monotonic_fallback(time);
        return true;
    }

    uint64_t minute_epoch_seconds;
    if (!pomodoro_local_datetime_to_utc(
            (pomodoro_date_t) {.year = year, .month = month, .day = day},
            hour, minute, 0, &minute_epoch_seconds)) {
        use_monotonic_fallback(time);
        return true;
    }
    time->epoch_seconds = minute_epoch_seconds + second;
    time->rtc_is_valid = true;
    time->local_date = (pomodoro_date_t) {
        .year = year,
        .month = month,
        .day = day,
    };
    last_valid_utc_epoch_seconds = time->epoch_seconds;
    last_valid_monotonic_microseconds = esp_timer_get_time();
    has_last_valid_time = true;
    return true;
}

bool pomodoro_board_resolve_local_date(uint64_t epoch_seconds,
                                       int8_t utc_offset_quarter_hours,
                                       pomodoro_date_t *date)
{
    return pomodoro_local_date_from_utc(
        epoch_seconds, utc_offset_quarter_hours, date);
}

bool pomodoro_board_write_utc_time(uint64_t utc_epoch_seconds)
{
    pomodoro_date_t date;
    if (board.rtc == NULL ||
        !pomodoro_local_date_from_utc(utc_epoch_seconds, 0, &date) ||
        date.year < 2020U || date.year > 2069U) {
        return false;
    }
    uint32_t seconds_of_day = (uint32_t) (utc_epoch_seconds % 86400U);
    uint8_t raw[] = {
        to_bcd((uint8_t) (seconds_of_day % 60U)),
        to_bcd((uint8_t) ((seconds_of_day / 60U) % 60U)),
        to_bcd((uint8_t) (seconds_of_day / 3600U)),
        to_bcd(date.day),
        0U,
        to_bcd(date.month),
        to_bcd((uint8_t) (date.year - 1970U)),
    };
    if (write_registers(board.rtc, PCF85063_SECONDS_REGISTER, raw,
                        sizeof(raw)) != ESP_OK) {
        ESP_LOGE(TAG, "RTC write failed");
        return false;
    }
    has_last_valid_time = false;
    return true;
}

bool pomodoro_board_set_display_brightness(uint8_t percent)
{
    if (board.panel_io == NULL || percent < 1U || percent > 100U) {
        return false;
    }
    uint8_t value = (uint8_t) ((uint16_t) percent * 255U / 100U);
    return esp_lcd_panel_io_tx_param(board.panel_io, 0x51, &value,
                                     sizeof(value)) == ESP_OK;
}

bool pomodoro_board_read_device_status(
    pomodoro_application_device_status_t *status)
{
    uint8_t status1;
    uint8_t status2;
    uint8_t battery_percent;
    if (status == NULL || board.pmic == NULL ||
        read_register(board.pmic, AXP2101_STATUS1_REGISTER, &status1, 1) !=
            ESP_OK ||
        read_register(board.pmic, AXP2101_STATUS2_REGISTER, &status2, 1) !=
            ESP_OK ||
        read_register(board.pmic, AXP2101_BATTERY_PERCENT_REGISTER,
                      &battery_percent, 1) != ESP_OK ||
        battery_percent > 100U) {
        return false;
    }
    *status = (pomodoro_application_device_status_t) {
        .battery_percent = battery_percent,
        .charging = (status2 >> 5U) == 0x01U,
        .usb_power = (status1 & 0x20U) != 0U && (status2 & 0x08U) == 0U,
        .low_battery = battery_percent <= 15U,
    };
    return true;
}

bool pomodoro_board_set_display_awake(bool awake)
{
    return board.panel != NULL &&
           esp_lcd_panel_disp_on_off(board.panel, awake) == ESP_OK;
}

bool pomodoro_board_power_off(void)
{
    uint8_t config;
    if (board.pmic == NULL ||
        read_register(board.pmic, AXP2101_COMMON_CONFIG_REGISTER, &config,
                      1) != ESP_OK) {
        return false;
    }
    return write_register(board.pmic, AXP2101_COMMON_CONFIG_REGISTER,
                          config | 0x01U) == ESP_OK;
}

esp_err_t pomodoro_board_initialize_controls(void)
{
    gpio_config_t config = {
        .pin_bit_mask = (UINT64_C(1) << KEY_VOLUME_UP) |
                        (UINT64_C(1) << BOOT_VOLUME_DOWN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t result = gpio_config(&config);
    if (result != ESP_OK) {
        return result;
    }
    uint8_t interrupts;
    result = read_register(board.pmic, AXP2101_INTERRUPT_ENABLE2_REGISTER,
                           &interrupts, 1);
    if (result == ESP_OK) {
        result = write_register(
            board.pmic, AXP2101_INTERRUPT_ENABLE2_REGISTER,
            interrupts | AXP2101_POWER_SHORT_PRESS_BIT |
                AXP2101_POWER_LONG_PRESS_BIT);
    }
    if (result == ESP_OK) {
        result = write_register(
            board.pmic, AXP2101_INTERRUPT_STATUS2_REGISTER,
            AXP2101_POWER_SHORT_PRESS_BIT |
                AXP2101_POWER_LONG_PRESS_BIT);
    }
    return result;
}

bool pomodoro_board_read_volume_buttons(bool *increase, bool *decrease)
{
    if (increase == NULL || decrease == NULL) {
        return false;
    }
    *increase = gpio_get_level(KEY_VOLUME_UP) == 0;
    *decrease = gpio_get_level(BOOT_VOLUME_DOWN) == 0;
    return true;
}

bool pomodoro_board_read_power_events(bool *short_press, bool *long_press)
{
    uint8_t events;
    if (short_press == NULL || long_press == NULL ||
        read_register(board.pmic, AXP2101_INTERRUPT_STATUS2_REGISTER,
                      &events, 1) != ESP_OK) {
        return false;
    }
    uint8_t relevant = events & (AXP2101_POWER_SHORT_PRESS_BIT |
                                 AXP2101_POWER_LONG_PRESS_BIT);
    *short_press = (relevant & AXP2101_POWER_SHORT_PRESS_BIT) != 0U;
    *long_press = (relevant & AXP2101_POWER_LONG_PRESS_BIT) != 0U;
    return relevant == 0U || write_register(
        board.pmic, AXP2101_INTERRUPT_STATUS2_REGISTER, relevant) == ESP_OK;
}

esp_err_t pomodoro_board_initialize_audio(void)
{
    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(
        I2S_NUM_AUTO, I2S_ROLE_MASTER);
    channel_config.auto_clear = true;
    esp_err_t result = i2s_new_channel(&channel_config, &audio_tx_channel,
                                       NULL);
    if (result != ESP_OK) {
        return result;
    }
    i2s_std_config_t standard_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = AUDIO_MCLK,
            .bclk = AUDIO_BCLK,
            .ws = AUDIO_WS,
            .dout = AUDIO_DOUT,
            .din = AUDIO_DIN,
        },
    };
    result = i2s_channel_init_std_mode(audio_tx_channel, &standard_config);
    if (result != ESP_OK) {
        return result;
    }
    result = i2s_channel_enable(audio_tx_channel);
    if (result != ESP_OK) {
        return result;
    }

    audio_codec_i2s_cfg_t data_config = {
        .port = I2S_NUM_0,
        .tx_handle = audio_tx_channel,
    };
    const audio_codec_data_if_t *data_interface =
        audio_codec_new_i2s_data(&data_config);
    audio_codec_i2c_cfg_t control_config = {
        .port = I2C_NUM_0,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = board.i2c_bus,
    };
    const audio_codec_ctrl_if_t *control_interface =
        audio_codec_new_i2c_ctrl(&control_config);
    const audio_codec_gpio_if_t *gpio_interface = audio_codec_new_gpio();
    es8311_codec_cfg_t codec_config = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .ctrl_if = control_interface,
        .gpio_if = gpio_interface,
        .pa_pin = GPIO_NUM_NC,
        .use_mclk = true,
        .hw_gain = {.pa_gain = 6},
    };
    const audio_codec_if_t *codec_interface = es8311_codec_new(&codec_config);
    if (data_interface == NULL || control_interface == NULL ||
        gpio_interface == NULL || codec_interface == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_codec_dev_cfg_t device_config = {
        .codec_if = codec_interface,
        .data_if = data_interface,
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    };
    audio_device = esp_codec_dev_new(&device_config);
    if (audio_device == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_codec_dev_sample_info_t sample = {
        .sample_rate = AUDIO_SAMPLE_RATE,
        .channel = 2,
        .bits_per_sample = 16,
    };
    return esp_codec_dev_open(audio_device, &sample) == ESP_CODEC_DEV_OK
        ? ESP_OK
        : ESP_FAIL;
}

static bool write_tone(uint16_t frequency_hz, uint16_t milliseconds,
                       uint8_t volume)
{
    static const int16_t waveform[] = {
        0, 6393, 11819, 15446, 16720, 15446, 11819, 6393,
        0, -6393, -11819, -15446, -16720, -15446, -11819, -6393,
    };
    int16_t frames[128U * 2U];
    uint32_t phase = 0U;
    uint32_t phase_step = ((uint32_t) frequency_hz << 16U) /
        AUDIO_SAMPLE_RATE;
    uint32_t frames_remaining =
        AUDIO_SAMPLE_RATE * milliseconds / 1000U;
    if (esp_codec_dev_set_out_vol(audio_device, volume * 20U) !=
        ESP_CODEC_DEV_OK) {
        return false;
    }
    while (frames_remaining > 0U) {
        uint32_t count = frames_remaining > 128U ? 128U : frames_remaining;
        for (uint32_t index = 0; index < count; index++) {
            int16_t sample = waveform[(phase >> 12U) & 0x0FU];
            frames[index * 2U] = sample;
            frames[index * 2U + 1U] = sample;
            phase += phase_step;
        }
        if (esp_codec_dev_write(audio_device, frames,
                                count * 2U * sizeof(int16_t)) !=
            ESP_CODEC_DEV_OK) {
            return false;
        }
        frames_remaining -= count;
    }
    return true;
}

static void play_tone_task(void *context)
{
    (void) context;
    bool focus = queued_focus_tone;
    uint8_t volume = queued_tone_volume;
    if (focus) {
        (void) write_tone(660U, 120U, volume);
        vTaskDelay(pdMS_TO_TICKS(60));
        (void) write_tone(880U, 120U, volume);
    } else {
        (void) write_tone(523U, 180U, volume);
    }
    audio_busy = false;
    vTaskDelete(NULL);
}

bool pomodoro_board_play_completion_tone(bool focus, uint8_t volume)
{
    if (volume == 0U) {
        return true;
    }
    if (audio_device == NULL || audio_busy) {
        return false;
    }
    queued_focus_tone = focus;
    queued_tone_volume = volume;
    audio_busy = true;
    if (xTaskCreate(play_tone_task, "pomodoro_tone", 3072, NULL, 2, NULL) !=
        pdPASS) {
        audio_busy = false;
        return false;
    }
    return true;
}

static void round_display_area(lv_area_t *area)
{
    area->x1 &= ~1;
    area->y1 &= ~1;
    area->x2 |= 1;
    area->y2 |= 1;
    if (area->x2 >= (int32_t) DISPLAY_WIDTH) {
        area->x2 = DISPLAY_WIDTH - 1U;
    }
    if (area->y2 >= (int32_t) DISPLAY_HEIGHT) {
        area->y2 = DISPLAY_HEIGHT - 1U;
    }
}

esp_err_t pomodoro_board_initialize_i2c(void)
{
    i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BOARD_I2C_SDA,
        .scl_io_num = BOARD_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 1,
    };
    esp_err_t result = i2c_new_master_bus(&config, &board.i2c_bus);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus initialization failed: %s", esp_err_to_name(result));
        return result;
    }
    result = add_i2c_device(AXP2101_ADDRESS, &board.pmic);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "AXP2101 initialization failed: %s", esp_err_to_name(result));
    }
    return result;
}

static esp_err_t reset_display_power(void)
{
    uint8_t enables;
    esp_err_t result = read_register(board.pmic, 0x90, &enables, 1);
    if (result != ESP_OK) {
        return result;
    }
    result = write_register(board.pmic, 0x90, enables | 0x04U);
    vTaskDelay(pdMS_TO_TICKS(100));
    if (result == ESP_OK) {
        result = write_register(board.pmic, 0x90, enables & (uint8_t) ~0x04U);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    if (result == ESP_OK) {
        result = write_register(board.pmic, 0x90, enables | 0x04U);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    return result;
}

esp_err_t pomodoro_board_initialize_display(void)
{
    spi_bus_config_t bus_config =
        SH8601_PANEL_BUS_QSPI_CONFIG(LCD_PCLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3,
                                     DISPLAY_WIDTH * (DRAW_BUFFER_ROWS + 2U) * 2U);
    esp_err_t result = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (result != ESP_OK) {
        return result;
    }
    esp_lcd_panel_io_spi_config_t io_config =
        SH8601_PANEL_IO_QSPI_CONFIG(LCD_CS, NULL, NULL);
    result = esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &board.panel_io);
    if (result != ESP_OK) {
        return result;
    }
    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_commands,
        .init_cmds_size = sizeof(lcd_init_commands) / sizeof(lcd_init_commands[0]),
        .flags.use_qspi_interface = 1,
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    result = esp_lcd_new_panel_sh8601(board.panel_io, &panel_config, &board.panel);
    if (result != ESP_OK) {
        return result;
    }
    result = reset_display_power();
    if (result != ESP_OK) {
        return result;
    }
    result = esp_lcd_panel_init(board.panel);
    if (result != ESP_OK) {
        return result;
    }

    lvgl_port_cfg_t lvgl_config = ESP_LVGL_PORT_INIT_CONFIG();
    result = lvgl_port_init(&lvgl_config);
    if (result != ESP_OK) {
        return result;
    }
    lvgl_port_display_cfg_t display_config = {
        .io_handle = board.panel_io,
        .panel_handle = board.panel,
        .buffer_size = DISPLAY_WIDTH * DRAW_BUFFER_ROWS,
        .double_buffer = false,
        .hres = DISPLAY_WIDTH,
        .vres = DISPLAY_HEIGHT,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .rounder_cb = round_display_area,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        },
    };
    board.display = lvgl_port_add_disp(&display_config);
    if (board.display == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG,
             "LVGL draw buffer=%u pixels (%u bytes), single buffered",
             DISPLAY_WIDTH * DRAW_BUFFER_ROWS,
             DISPLAY_WIDTH * DRAW_BUFFER_ROWS * 2U);
    return ESP_OK;
}

esp_err_t pomodoro_board_initialize_touch(void)
{
    esp_lcd_panel_io_i2c_config_t io_config =
        ESP_LCD_TOUCH_IO_I2C_CST9217_CONFIG();
    io_config.scl_speed_hz = 400000;
    esp_lcd_panel_io_handle_t touch_io = NULL;
    esp_err_t result = esp_lcd_new_panel_io_i2c(board.i2c_bus, &io_config,
                                                 &touch_io);
    if (result != ESP_OK) {
        return result;
    }
    esp_lcd_touch_config_t config = {
        .x_max = DISPLAY_WIDTH - 1U,
        .y_max = DISPLAY_HEIGHT - 1U,
        .rst_gpio_num = TOUCH_RESET,
        .int_gpio_num = TOUCH_INTERRUPT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 1,
            .mirror_x = 0,
            .mirror_y = 1,
        },
    };
    result = esp_lcd_touch_new_i2c_cst9217(touch_io, &config, &board.touch);
    if (result != ESP_OK) {
        return result;
    }
    lvgl_port_touch_cfg_t touch_config = {
        .disp = board.display,
        .handle = board.touch,
    };
    board.touch_input = lvgl_port_add_touch(&touch_config);
    return board.touch_input != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t pomodoro_board_initialize_rtc(void)
{
    return add_i2c_device(PCF85063_ADDRESS, &board.rtc);
}
