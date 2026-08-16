#ifndef POMODORO_PLATFORM_H
#define POMODORO_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t (*epoch_seconds)(void *context);
    uint64_t (*monotonic_milliseconds)(void *context);
    bool (*rtc_is_valid)(void *context);
} pomodoro_clock_service_t;

typedef struct {
    bool (*load)(void *context, const char *key, void *value, size_t size);
    bool (*save)(void *context, const char *key, const void *value, size_t size);
} pomodoro_storage_service_t;

typedef struct {
    void (*set_brightness_percent)(void *context, uint8_t percent);
    void (*set_display_awake)(void *context, bool awake);
} pomodoro_display_service_t;

typedef struct {
    void (*play_focus_complete)(void *context, uint8_t volume);
    void (*play_break_complete)(void *context, uint8_t volume);
} pomodoro_audio_service_t;

typedef struct {
    void *context;
    pomodoro_clock_service_t clock;
    pomodoro_storage_service_t storage;
    pomodoro_display_service_t display;
    pomodoro_audio_service_t audio;
} pomodoro_platform_t;

const pomodoro_platform_t *pomodoro_platform_get(void);

#ifdef __cplusplus
}
#endif

#endif

