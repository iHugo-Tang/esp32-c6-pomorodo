#include "pomodoro_orientation.h"

#include <math.h>
#include <stddef.h>

void pomodoro_orientation_resolver_init(pomodoro_orientation_resolver_t *resolver)
{
    *resolver = (pomodoro_orientation_resolver_t) {
        .current = POMODORO_UI_ORIENTATION_0,
    };
}

bool pomodoro_orientation_classify(float gx_g, float gy_g,
                                   pomodoro_ui_orientation_t *orientation)
{
    /* Orientation N means "rotate the display N degrees clockwise": gy>0 is
     * top edge up (0°), gy<0 is upside down (180°), gx>0 is right edge down
     * (90°), gx<0 is left edge down (270°). */
    if (orientation == NULL) {
        return false;
    }
    float ax = fabsf(gx_g);
    float ay = fabsf(gy_g);
    if (ay >= POMODORO_ORIENTATION_MIN_GRAVITY_G &&
        ay - ax >= POMODORO_ORIENTATION_DOMINANCE_G) {
        *orientation = gy_g > 0.0f
            ? POMODORO_UI_ORIENTATION_0
            : POMODORO_UI_ORIENTATION_180;
        return true;
    }
    if (ax >= POMODORO_ORIENTATION_MIN_GRAVITY_G &&
        ax - ay >= POMODORO_ORIENTATION_DOMINANCE_G) {
        *orientation = gx_g > 0.0f
            ? POMODORO_UI_ORIENTATION_90
            : POMODORO_UI_ORIENTATION_270;
        return true;
    }
    return false;
}

pomodoro_ui_orientation_t pomodoro_orientation_resolver_current(
    const pomodoro_orientation_resolver_t *resolver)
{
    return resolver->current;
}

static void apply_candidate(pomodoro_orientation_resolver_t *resolver,
                            pomodoro_ui_orientation_t candidate,
                            pomodoro_orientation_event_t *event)
{
    resolver->current = candidate;
    resolver->candidate_count = 0U;
    resolver->has_pending = false;
    event->orientation_changed = true;
    event->orientation = candidate;
}

static void classify_and_apply(pomodoro_orientation_resolver_t *resolver,
                               const pomodoro_orientation_accel_t *accel,
                               bool touch_active,
                               pomodoro_orientation_event_t *event)
{
    pomodoro_ui_orientation_t candidate;
    if (!pomodoro_orientation_classify(accel->gx_g, accel->gy_g, &candidate)) {
        resolver->candidate_count = 0U;
        return;
    }
    if (candidate != resolver->candidate) {
        resolver->candidate = candidate;
        resolver->candidate_count = 1U;
    } else if (resolver->candidate_count < 255U) {
        ++resolver->candidate_count;
    }
    if (resolver->candidate_count >= POMODORO_ORIENTATION_STABILITY_SAMPLES) {
        if (candidate == resolver->current) {
            resolver->has_pending = false;
        } else if (touch_active) {
            /* A stable direction reached during a touch is retained as
             * pending and applied on release, even if later samples become
             * ambiguous while the touch is still active. */
            resolver->has_pending = true;
            resolver->pending = candidate;
        } else {
            apply_candidate(resolver, candidate, event);
        }
    } else if (resolver->has_pending && !touch_active &&
               candidate == resolver->pending) {
        apply_candidate(resolver, candidate, event);
    }
}

void pomodoro_orientation_resolver_sample(
    pomodoro_orientation_resolver_t *resolver, bool read_succeeded,
    const pomodoro_orientation_accel_t *accel, bool touch_active,
    pomodoro_orientation_event_t *event)
{
    event->orientation_changed = false;
    event->orientation = resolver->current;
    event->warning_started = false;
    event->retry_sensor = false;

    if (!resolver->unavailable) {
        if (!read_succeeded) {
            resolver->candidate_count = 0U;
            if (++resolver->read_failures >=
                POMODORO_ORIENTATION_MAX_READ_FAILURES) {
                resolver->unavailable = true;
                resolver->retry_ticks = 0U;
                if (!resolver->warning_emitted) {
                    resolver->warning_emitted = true;
                    event->warning_started = true;
                }
            }
            return;
        }
        resolver->read_failures = 0U;
        if (accel != NULL) {
            classify_and_apply(resolver, accel, touch_active, event);
        }
        return;
    }

    if (++resolver->retry_ticks >= POMODORO_ORIENTATION_RETRY_TICKS) {
        resolver->retry_ticks = 0U;
        event->retry_sensor = true;
    }
}

void pomodoro_orientation_resolver_report_reinit(
    pomodoro_orientation_resolver_t *resolver, bool succeeded)
{
    /* Recovery is gated on a successful re-initialization retry: reads
     * recovering on their own do not resume classification. */
    if (resolver != NULL && resolver->unavailable && succeeded) {
        resolver->unavailable = false;
        resolver->read_failures = 0U;
        resolver->candidate_count = 0U;
    }
}
