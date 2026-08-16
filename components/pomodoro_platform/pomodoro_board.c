#include "pomodoro_board.h"

#include <stddef.h>

#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"
#include "esp_lcd_touch_cst9217.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
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
#define DISPLAY_WIDTH 480U
#define DISPLAY_HEIGHT 480U
#define DRAW_BUFFER_ROWS 20U
#define I2C_TIMEOUT_MS 250
#define AXP2101_ADDRESS 0x34U
#define PCF85063_ADDRESS 0x51U
#define PCF85063_SECONDS_REGISTER 0x04U

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

static uint8_t from_bcd(uint8_t value)
{
    return (uint8_t) (((value >> 4U) * 10U) + (value & 0x0FU));
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

static int64_t days_since_unix_epoch(int32_t year, uint8_t month, uint8_t day)
{
    year -= month <= 2U;
    int32_t era = (year >= 0 ? year : year - 399) / 400;
    uint32_t year_of_era = (uint32_t) (year - era * 400);
    uint32_t day_of_year =
        (153U * (uint32_t) (month + (month > 2U ? -3 : 9)) + 2U) / 5U +
        day - 1U;
    uint32_t day_of_era = year_of_era * 365U + year_of_era / 4U -
        year_of_era / 100U + day_of_year;
    return (int64_t) era * 146097 + (int64_t) day_of_era - 719468;
}

bool pomodoro_board_read_time(pomodoro_application_time_t *time)
{
    uint8_t raw[7];
    if (time == NULL || board.rtc == NULL ||
        read_register(board.rtc, PCF85063_SECONDS_REGISTER, raw,
                      sizeof(raw)) != ESP_OK) {
        ESP_LOGE(TAG, "RTC read failed");
        return false;
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
        return false;
    }

    int64_t days = days_since_unix_epoch(year, month, day);
    time->epoch_seconds = (uint64_t) (days * 86400 + hour * 3600U +
                                      minute * 60U + second);
    time->rtc_is_valid = true;
    time->local_date = (pomodoro_date_t) {
        .year = year,
        .month = month,
        .day = day,
    };
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
