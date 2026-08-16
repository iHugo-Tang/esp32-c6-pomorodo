#ifndef POMODORO_PLATFORM_H
#define POMODORO_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#include "pomodoro_persistence.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t (*epoch_seconds)(void *context);
    uint64_t (*monotonic_milliseconds)(void *context);
    bool (*rtc_is_valid)(void *context);
} pomodoro_clock_service_t;

typedef struct {
    pomodoro_journal_t journal;
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
bool pomodoro_nvs_journal_init(pomodoro_journal_t *journal);

#ifdef __cplusplus
}
#endif

#endif
