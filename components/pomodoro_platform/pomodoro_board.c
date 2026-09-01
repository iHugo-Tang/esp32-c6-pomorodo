#include "pomodoro_board.h"
#include "pomodoro_audio_priority.h"

#include <stddef.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/usb_serial_jtag.h"
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
#include "freertos/semphr.h"
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
#define KEY_VOLUME_UP GPIO_NUM_10
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
#define SCREENSHOT_STALL_TIMEOUT_US 30000000LL
#define I2C_TIMEOUT_MS 250
#define AXP2101_ADDRESS 0x34U
#define PCF85063_ADDRESS 0x51U
#define SH8601_QSPI_WRITE_COMMAND_OPCODE 0x02U
#define SH8601_BRIGHTNESS_COMMAND 0x51U
#define PCF85063_SECONDS_REGISTER 0x04U
#define AXP2101_STATUS1_REGISTER 0x00U
#define AXP2101_STATUS2_REGISTER 0x01U
#define AXP2101_COMMON_CONFIG_REGISTER 0x10U
#define AXP2101_BATTERY_PERCENT_REGISTER 0xA4U
#define AXP2101_INTERRUPT_ENABLE2_REGISTER 0x41U
#define AXP2101_INTERRUPT_STATUS2_REGISTER 0x49U
#define AXP2101_POWER_SHORT_PRESS_BIT 0x08U
#define AXP2101_POWER_LONG_PRESS_BIT 0x04U

#define QMI8658_ADDRESS 0x6BU
#define QMI8658_WHO_AM_I_REG 0x00U
#define QMI8658_CTRL1_REG 0x02U
#define QMI8658_CTRL2_REG 0x03U
#define QMI8658_CTRL7_REG 0x08U
#define QMI8658_AX_L_REG 0x35U
#define QMI8658_WHO_AM_I_VALUE 0x05U
#define QMI8658_ACCEL_LSB_PER_G 4096.0f /* ±8g full scale */
#define QMI8658_ACCEL_CTRL2 0x27U        /* ±8g, 62.5Hz output rate */

typedef struct {
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_dev_handle_t pmic;
    i2c_master_dev_handle_t rtc;
    i2c_master_dev_handle_t imu;
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t panel;
    esp_lcd_touch_handle_t touch;
    lv_display_t *display;
    lv_indev_t *touch_input;
} board_context_t;

static const char *TAG = "pomodoro_board";
extern const uint8_t warning_pcm_start[]
    asm("_binary_warning_pcm_start");
extern const uint8_t warning_pcm_end[]
    asm("_binary_warning_pcm_end");
extern const uint8_t rest_pcm_start[] asm("_binary_rest_pcm_start");
extern const uint8_t rest_pcm_end[] asm("_binary_rest_pcm_end");
static board_context_t board;
static uint64_t last_valid_wall_time_seconds;
static int64_t last_valid_monotonic_microseconds;
static bool has_last_valid_time;
static i2s_chan_handle_t audio_tx_channel;
static esp_codec_dev_handle_t audio_device;
static pomodoro_audio_priority_t audio_priority;
static uint8_t active_audio_volume;
static uint8_t pending_completion_volume;
static SemaphoreHandle_t audio_request_mutex;
static portMUX_TYPE audio_lock = portMUX_INITIALIZER_UNLOCKED;
static bool screenshot_active;
static int64_t screenshot_last_progress_us;

static bool write_usb_all(const void *buffer, size_t size)
{
    const uint8_t *bytes = buffer;
    while (size > 0U) {
        size_t chunk = size > POMODORO_SCREENSHOT_USB_CHUNK_BYTES
            ? POMODORO_SCREENSHOT_USB_CHUNK_BYTES
            : size;
        int written = usb_serial_jtag_write_bytes(bytes, chunk,
                                                   pdMS_TO_TICKS(100));
        if (written <= 0) {
            return false;
        }
        bytes += written;
        size -= (size_t) written;
    }
    return true;
}

static void capture_flush(lv_event_t *event)
{
    if (!screenshot_active) {
        return;
    }
    screenshot_last_progress_us = esp_timer_get_time();
    lv_area_t *area = lv_event_get_param(event);
    lv_draw_buf_t *draw_buffer = lv_display_get_buf_active(board.display);
    if (area == NULL || draw_buffer == NULL || draw_buffer->data == NULL) {
        screenshot_active = false;
        return;
    }
    uint16_t x = (uint16_t) area->x1;
    uint16_t y = (uint16_t) area->y1;
    uint16_t width = (uint16_t) lv_area_get_width(area);
    uint16_t height = (uint16_t) lv_area_get_height(area);
    uint8_t header[] = {
        POMODORO_SCREENSHOT_CHUNK_MAGIC[0],
        POMODORO_SCREENSHOT_CHUNK_MAGIC[1],
        (uint8_t) x, (uint8_t) (x >> 8U),
        (uint8_t) y, (uint8_t) (y >> 8U),
        (uint8_t) width, (uint8_t) (width >> 8U),
        (uint8_t) height, (uint8_t) (height >> 8U),
    };
    size_t pixel_bytes = (size_t) width * height *
        POMODORO_SCREENSHOT_BYTES_PER_PIXEL;
    if (!write_usb_all(header, sizeof(header)) ||
        !write_usb_all(draw_buffer->data, pixel_bytes)) {
        screenshot_active = false;
        return;
    }
    if (lv_display_flush_is_last(board.display)) {
        static const char done[] = POMODORO_SCREENSHOT_DONE_MAGIC;
        screenshot_active = false;
        (void) write_usb_all(done, sizeof(done) - 1U);
    }
}

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
        .wall_time_seconds = has_last_valid_time
            ? last_valid_wall_time_seconds + elapsed_seconds
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

    uint64_t minute_wall_time_seconds;
    if (!pomodoro_local_datetime_to_wall_time(
            (pomodoro_date_t) {.year = year, .month = month, .day = day},
            hour, minute, &minute_wall_time_seconds)) {
        use_monotonic_fallback(time);
        return true;
    }
    time->wall_time_seconds = minute_wall_time_seconds + second;
    time->rtc_is_valid = true;
    time->local_date = (pomodoro_date_t) {
        .year = year,
        .month = month,
        .day = day,
    };
    last_valid_wall_time_seconds = time->wall_time_seconds;
    last_valid_monotonic_microseconds = esp_timer_get_time();
    has_last_valid_time = true;
    return true;
}

bool pomodoro_board_resolve_local_date(uint64_t wall_time_seconds,
                                       pomodoro_date_t *date)
{
    return pomodoro_local_date_from_wall_time(wall_time_seconds, date);
}

bool pomodoro_board_write_time(uint64_t wall_time_seconds)
{
    pomodoro_date_t date;
    if (board.rtc == NULL ||
        !pomodoro_local_date_from_wall_time(wall_time_seconds, &date) ||
        date.year < 2020U || date.year > 2069U) {
        return false;
    }
    uint32_t seconds_of_day = (uint32_t) (wall_time_seconds % 86400U);
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
    uint32_t command = (SH8601_QSPI_WRITE_COMMAND_OPCODE << 24U) |
                       (SH8601_BRIGHTNESS_COMMAND << 8U);
    return esp_lcd_panel_io_tx_param(board.panel_io, command, &value,
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
    pomodoro_audio_priority_init(&audio_priority);
    active_audio_volume = 0U;
    pending_completion_volume = 0U;
    audio_request_mutex = NULL;
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
    if (esp_codec_dev_open(audio_device, &sample) != ESP_CODEC_DEV_OK) {
        return ESP_FAIL;
    }
    audio_request_mutex = xSemaphoreCreateMutex();
    return audio_request_mutex != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

static bool write_audio_clip(const uint8_t *start, const uint8_t *end,
                             uint8_t volume)
{
    int16_t frames[128U * 2U];
    if (start == NULL || end == NULL || end < start) {
        return false;
    }
    size_t byte_count = (size_t) (end - start);
    size_t sample_offset = 0U;
    size_t sample_count = byte_count / sizeof(int16_t);
    if ((byte_count % sizeof(int16_t)) != 0U) {
        return false;
    }
    if (esp_codec_dev_set_out_vol(audio_device, volume * 20U) !=
        ESP_CODEC_DEV_OK) {
        return false;
    }
    while (sample_offset < sample_count) {
        size_t remaining = sample_count - sample_offset;
        size_t count = remaining > 128U ? 128U : remaining;
        for (size_t index = 0; index < count; index++) {
            size_t byte_offset = (sample_offset + index) * sizeof(int16_t);
            uint16_t encoded = (uint16_t) start[byte_offset] |
                ((uint16_t) start[byte_offset + 1U] << 8U);
            int16_t sample = (int16_t) encoded;
            frames[index * 2U] = sample;
            frames[index * 2U + 1U] = sample;
        }
        if (esp_codec_dev_write(audio_device, frames,
                                count * 2U * sizeof(int16_t)) !=
            ESP_CODEC_DEV_OK) {
            return false;
        }
        sample_offset += count;
    }
    return true;
}

static void play_audio_task(void *context)
{
    (void) context;
    bool play_next = true;
    while (play_next) {
        portENTER_CRITICAL(&audio_lock);
        pomodoro_audio_cue_t cue =
            pomodoro_audio_priority_active(&audio_priority);
        uint8_t volume = active_audio_volume;
        portEXIT_CRITICAL(&audio_lock);

        if (cue == POMODORO_AUDIO_CUE_WAITING) {
            (void) write_audio_clip(warning_pcm_start, warning_pcm_end,
                                    volume);
        } else if (cue == POMODORO_AUDIO_CUE_FOCUS_COMPLETE ||
                   cue == POMODORO_AUDIO_CUE_BREAK_COMPLETE) {
            (void) write_audio_clip(rest_pcm_start, rest_pcm_end, volume);
        }

        portENTER_CRITICAL(&audio_lock);
        if (pomodoro_audio_priority_finish(&audio_priority)) {
            active_audio_volume = pending_completion_volume;
            pending_completion_volume = 0U;
        } else {
            active_audio_volume = 0U;
            play_next = false;
        }
        portEXIT_CRITICAL(&audio_lock);
    }
    vTaskDelete(NULL);
}

static bool request_audio_cue(pomodoro_audio_cue_t cue, uint8_t volume)
{
    if (cue == POMODORO_AUDIO_CUE_NONE ||
        (volume > 0U && audio_device == NULL) ||
        audio_request_mutex == NULL) {
        return false;
    }

    if (xSemaphoreTake(audio_request_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    portENTER_CRITICAL(&audio_lock);
    pomodoro_audio_request_result_t request =
        pomodoro_audio_priority_request(&audio_priority, cue, volume);
    if (request == POMODORO_AUDIO_REQUEST_START) {
        active_audio_volume = volume;
    } else if (request == POMODORO_AUDIO_REQUEST_QUEUED) {
        pending_completion_volume = volume;
    }
    portEXIT_CRITICAL(&audio_lock);

    if (request == POMODORO_AUDIO_REQUEST_SILENT) {
        xSemaphoreGive(audio_request_mutex);
        return true;
    }
    if (request != POMODORO_AUDIO_REQUEST_START) {
        xSemaphoreGive(audio_request_mutex);
        return request == POMODORO_AUDIO_REQUEST_QUEUED;
    }
    if (xTaskCreate(play_audio_task, "pomodoro_audio", 3072, NULL, 2, NULL) !=
        pdPASS) {
        portENTER_CRITICAL(&audio_lock);
        pomodoro_audio_priority_init(&audio_priority);
        active_audio_volume = 0U;
        pending_completion_volume = 0U;
        portEXIT_CRITICAL(&audio_lock);
        xSemaphoreGive(audio_request_mutex);
        return false;
    }
    xSemaphoreGive(audio_request_mutex);
    return true;
}

bool pomodoro_board_play_focus_completion_sound(uint8_t volume)
{
    return request_audio_cue(POMODORO_AUDIO_CUE_FOCUS_COMPLETE, volume);
}

bool pomodoro_board_play_break_completion_sound(uint8_t volume)
{
    return request_audio_cue(POMODORO_AUDIO_CUE_BREAK_COMPLETE, volume);
}

bool pomodoro_board_play_waiting_sound(uint8_t volume)
{
    return request_audio_cue(POMODORO_AUDIO_CUE_WAITING, volume);
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

esp_err_t pomodoro_board_initialize_orientation(void)
{
    if (board.imu == NULL) {
        i2c_device_config_t config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = QMI8658_ADDRESS,
            .scl_speed_hz = 400000,
        };
        esp_err_t result = i2c_master_bus_add_device(board.i2c_bus, &config,
                                                     &board.imu);
        if (result != ESP_OK) {
            return result;
        }
    }
    uint8_t who_am_i;
    if (read_register(board.imu, QMI8658_WHO_AM_I_REG, &who_am_i, 1) !=
            ESP_OK ||
        who_am_i != QMI8658_WHO_AM_I_VALUE) {
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t result = write_register(board.imu, QMI8658_CTRL1_REG, 0x60U);
    if (result == ESP_OK) {
        result = write_register(board.imu, QMI8658_CTRL2_REG,
                                QMI8658_ACCEL_CTRL2);
    }
    if (result == ESP_OK) {
        /* Accelerometer enabled; gyroscope and gestures stay disabled. */
        result = write_register(board.imu, QMI8658_CTRL7_REG, 0x01U);
    }
    if (result != ESP_OK) {
        return result;
    }
    ESP_LOGI(TAG, "QMI8658 accelerometer initialized");
    return ESP_OK;
}

bool pomodoro_board_read_acceleration(pomodoro_orientation_accel_t *accel)
{
    uint8_t raw[6];
    if (accel == NULL || board.imu == NULL ||
        read_register(board.imu, QMI8658_AX_L_REG, raw, sizeof(raw)) !=
            ESP_OK) {
        return false;
    }
    int16_t raw_x = (int16_t) ((uint16_t) raw[1] << 8U | raw[0]);
    int16_t raw_y = (int16_t) ((uint16_t) raw[3] << 8U | raw[2]);
    float ax_g = (float) raw_x / QMI8658_ACCEL_LSB_PER_G;
    float ay_g = (float) raw_y / QMI8658_ACCEL_LSB_PER_G;
    /* Calibrated on target board: sensor axes match the screen frame
     * (gx=screen right, gy=screen down). Top edge up reads gy~+1g,
     * right edge down reads gx~+1g. */
    accel->gx_g = ax_g;
    accel->gy_g = ay_g;
    return true;
}

bool pomodoro_board_is_touch_active(void)
{
    return board.touch_input != NULL &&
           lv_indev_get_state(board.touch_input) == LV_INDEV_STATE_PRESSED;
}

void pomodoro_board_set_display_rotation(pomodoro_ui_orientation_t orientation)
{
    if (board.display == NULL) {
        return;
    }
    lv_display_rotation_t rotation = LV_DISPLAY_ROTATION_0;
    if (orientation == POMODORO_UI_ORIENTATION_90) {
        rotation = LV_DISPLAY_ROTATION_90;
    } else if (orientation == POMODORO_UI_ORIENTATION_180) {
        rotation = LV_DISPLAY_ROTATION_180;
    } else if (orientation == POMODORO_UI_ORIENTATION_270) {
        rotation = LV_DISPLAY_ROTATION_270;
    }
    lv_display_set_rotation(board.display, rotation);
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
            .sw_rotate = true,
        },
    };
    board.display = lvgl_port_add_disp(&display_config);
    if (board.display == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_display_add_event_cb(board.display, capture_flush,
                            LV_EVENT_FLUSH_START, NULL);
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

esp_err_t pomodoro_board_initialize_usb_serial_jtag(void)
{
    static usb_serial_jtag_driver_config_t config =
        USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    esp_err_t result = usb_serial_jtag_driver_install(&config);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }
    return ESP_OK;
}

int pomodoro_board_read_usb_serial_jtag(uint8_t *buffer, size_t size)
{
    if (buffer == NULL || size == 0U) {
        return 0;
    }
    return usb_serial_jtag_read_bytes(buffer, (uint32_t) size, 0U);
}

int pomodoro_board_write_usb_serial_jtag(const void *buffer, size_t size)
{
    if (buffer == NULL || size == 0U) {
        return 0;
    }
    if (screenshot_active) {
        if (esp_timer_get_time() - screenshot_last_progress_us <
            SCREENSHOT_STALL_TIMEOUT_US) {
            return 0;
        }
        screenshot_active = false;
    }
    return usb_serial_jtag_write_bytes(buffer, size, 20U);
}

bool pomodoro_board_request_screenshot(void)
{
    if (board.display == NULL || screenshot_active) {
        return false;
    }
    static const uint8_t dimensions[] = {
        (uint8_t) DISPLAY_WIDTH, (uint8_t) (DISPLAY_WIDTH >> 8U),
        (uint8_t) DISPLAY_HEIGHT, (uint8_t) (DISPLAY_HEIGHT >> 8U),
    };
    screenshot_active = true;
    screenshot_last_progress_us = esp_timer_get_time();
    if (!write_usb_all(POMODORO_SCREENSHOT_HEADER_MAGIC,
                       POMODORO_SCREENSHOT_HEADER_MAGIC_LENGTH) ||
        !write_usb_all(dimensions, sizeof(dimensions))) {
        screenshot_active = false;
        return false;
    }
    lv_obj_invalidate(lv_display_get_screen_active(board.display));
    return true;
}
