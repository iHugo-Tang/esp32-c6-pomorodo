#ifndef POMODORO_ORIENTATION_H
#define POMODORO_ORIENTATION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    POMODORO_UI_ORIENTATION_0 = 0,
    POMODORO_UI_ORIENTATION_90,
    POMODORO_UI_ORIENTATION_180,
    POMODORO_UI_ORIENTATION_270,
} pomodoro_ui_orientation_t;

#define POMODORO_ORIENTATION_MIN_GRAVITY_G 0.70f
#define POMODORO_ORIENTATION_DOMINANCE_G 0.15f
#define POMODORO_ORIENTATION_STABILITY_SAMPLES 6U
#define POMODORO_ORIENTATION_MAX_READ_FAILURES 3U
/* Retry sensor initialization every 5s; sampling runs at 10Hz. */
#define POMODORO_ORIENTATION_RETRY_TICKS 50U

typedef struct {
    float gx_g; /* screen-right gravity component */
    float gy_g; /* screen-down gravity component */
} pomodoro_orientation_accel_t;

typedef struct {
    bool orientation_changed;
    pomodoro_ui_orientation_t orientation;
    bool warning_started;
    bool retry_sensor;
} pomodoro_orientation_event_t;

typedef struct {
    pomodoro_ui_orientation_t current;
    pomodoro_ui_orientation_t candidate;
    pomodoro_ui_orientation_t pending;
    uint8_t candidate_count;
    uint8_t read_failures;
    uint16_t retry_ticks;
    bool unavailable;
    bool warning_emitted;
    bool has_pending;
} pomodoro_orientation_resolver_t;

void pomodoro_orientation_resolver_init(pomodoro_orientation_resolver_t *resolver);

bool pomodoro_orientation_classify(float gx_g, float gy_g,
                                   pomodoro_ui_orientation_t *orientation);

void pomodoro_orientation_resolver_sample(
    pomodoro_orientation_resolver_t *resolver, bool read_succeeded,
    const pomodoro_orientation_accel_t *accel, bool touch_active,
    pomodoro_orientation_event_t *event);

void pomodoro_orientation_resolver_report_reinit(
    pomodoro_orientation_resolver_t *resolver, bool succeeded);

pomodoro_ui_orientation_t pomodoro_orientation_resolver_current(
    const pomodoro_orientation_resolver_t *resolver);

#ifdef __cplusplus
}
#endif

#endif
