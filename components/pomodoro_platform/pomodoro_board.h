#ifndef POMODORO_BOARD_H
#define POMODORO_BOARD_H

#include <stdbool.h>

#include "esp_err.h"
#include "pomodoro_application.h"
#include "pomodoro_orientation.h"

#define POMODORO_SCREENSHOT_COMMAND "POMO1 SCREENSHOT"
#define POMODORO_SCREENSHOT_COMMAND_LENGTH 16U
#define POMODORO_SCREENSHOT_HEADER_MAGIC "POMO1"
#define POMODORO_SCREENSHOT_HEADER_MAGIC_LENGTH 5U
#define POMODORO_SCREENSHOT_CHUNK_MAGIC "PX"
#define POMODORO_SCREENSHOT_DONE_MAGIC "DONE"
#define POMODORO_SCREENSHOT_BYTES_PER_PIXEL 2U
#define POMODORO_SCREENSHOT_USB_CHUNK_BYTES 256U

esp_err_t pomodoro_board_initialize_i2c(void);
esp_err_t pomodoro_board_initialize_orientation(void);
esp_err_t pomodoro_board_initialize_display(void);
esp_err_t pomodoro_board_initialize_touch(void);
esp_err_t pomodoro_board_initialize_rtc(void);
esp_err_t pomodoro_board_initialize_controls(void);
esp_err_t pomodoro_board_initialize_audio(void);
esp_err_t pomodoro_board_initialize_usb_serial_jtag(void);
int pomodoro_board_read_usb_serial_jtag(uint8_t *buffer, size_t size);
int pomodoro_board_write_usb_serial_jtag(const void *buffer, size_t size);
bool pomodoro_board_request_screenshot(void);
bool pomodoro_board_read_time(pomodoro_application_time_t *time);
bool pomodoro_board_resolve_local_date(uint64_t wall_time_seconds,
                                       pomodoro_date_t *date);
bool pomodoro_board_write_time(uint64_t wall_time_seconds);
bool pomodoro_board_set_display_brightness(uint8_t percent);
bool pomodoro_board_read_device_status(
    pomodoro_application_device_status_t *status);
bool pomodoro_board_set_display_awake(bool awake);
bool pomodoro_board_power_off(void);
bool pomodoro_board_read_volume_buttons(bool *increase, bool *decrease);
bool pomodoro_board_read_power_events(bool *short_press, bool *long_press);
bool pomodoro_board_play_focus_completion_sound(uint8_t volume);
bool pomodoro_board_play_break_completion_sound(uint8_t volume);
bool pomodoro_board_read_acceleration(pomodoro_orientation_accel_t *accel);
bool pomodoro_board_is_touch_active(void);
void pomodoro_board_set_display_rotation(pomodoro_ui_orientation_t orientation);

#endif
