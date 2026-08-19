#include "pomodoro_orientation.h"

#include <assert.h>
#include <stdio.h>

static void sample_valid(pomodoro_orientation_resolver_t *resolver,
                         float gx_g, float gy_g, bool touch_active,
                         pomodoro_orientation_event_t *event)
{
    pomodoro_orientation_accel_t accel = {.gx_g = gx_g, .gy_g = gy_g};
    pomodoro_orientation_resolver_sample(resolver, true, &accel, touch_active,
                                         event);
}

static void sample_failure(pomodoro_orientation_resolver_t *resolver,
                           bool touch_active,
                           pomodoro_orientation_event_t *event)
{
    pomodoro_orientation_resolver_sample(resolver, false, NULL, touch_active,
                                         event);
}

static void test_classify_all_four_directions(void)
{
    pomodoro_ui_orientation_t orientation;
    assert(pomodoro_orientation_classify(0.0f, 1.0f, &orientation));
    assert(orientation == POMODORO_UI_ORIENTATION_0);
    assert(pomodoro_orientation_classify(-1.0f, 0.0f, &orientation));
    assert(orientation == POMODORO_UI_ORIENTATION_270);
    assert(pomodoro_orientation_classify(0.0f, -1.0f, &orientation));
    assert(orientation == POMODORO_UI_ORIENTATION_180);
    assert(pomodoro_orientation_classify(1.0f, 0.0f, &orientation));
    assert(orientation == POMODORO_UI_ORIENTATION_90);

    assert(!pomodoro_orientation_classify(0.0f, 0.0f, &orientation));
    assert(!pomodoro_orientation_classify(0.8f, 0.8f, &orientation));
    assert(!pomodoro_orientation_classify(-0.6f, 0.6f, &orientation));
}

static void test_validity_and_dominance_thresholds(void)
{
    pomodoro_ui_orientation_t orientation;
    assert(!pomodoro_orientation_classify(0.0f, 0.69f, &orientation));
    assert(pomodoro_orientation_classify(0.0f, 0.70f, &orientation));
    assert(orientation == POMODORO_UI_ORIENTATION_0);
    assert(pomodoro_orientation_classify(0.64f, 0.80f, &orientation));
    assert(orientation == POMODORO_UI_ORIENTATION_0);
    assert(!pomodoro_orientation_classify(0.66f, 0.80f, &orientation));
    assert(!pomodoro_orientation_classify(-0.80f, 0.66f, &orientation));
}

static void test_six_consecutive_samples_rotate_once(void)
{
    pomodoro_orientation_resolver_t resolver;
    pomodoro_orientation_resolver_init(&resolver);
    pomodoro_orientation_event_t event;

    for (unsigned index = 0; index < 5U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, false, &event);
        assert(!event.orientation_changed);
    }
    sample_valid(&resolver, -1.0f, 0.0f, false, &event);
    assert(event.orientation_changed);
    assert(event.orientation == POMODORO_UI_ORIENTATION_270);
    assert(pomodoro_orientation_resolver_current(&resolver) ==
           POMODORO_UI_ORIENTATION_270);

    for (unsigned index = 0; index < 10U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, false, &event);
        assert(!event.orientation_changed);
    }
}

static void test_ambiguous_sample_resets_candidate(void)
{
    pomodoro_orientation_resolver_t resolver;
    pomodoro_orientation_resolver_init(&resolver);
    pomodoro_orientation_event_t event;

    for (unsigned index = 0; index < 5U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, false, &event);
    }
    sample_valid(&resolver, 0.0f, 0.0f, false, &event);
    assert(!event.orientation_changed);
    for (unsigned index = 0; index < 5U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, false, &event);
        assert(!event.orientation_changed);
    }
    sample_valid(&resolver, -1.0f, 0.0f, false, &event);
    assert(event.orientation_changed);
    assert(event.orientation == POMODORO_UI_ORIENTATION_270);
}

static void test_preserve_current_direction_on_ambiguous_placement(void)
{
    pomodoro_orientation_resolver_t resolver;
    pomodoro_orientation_resolver_init(&resolver);
    pomodoro_orientation_event_t event;

    for (unsigned index = 0; index < 6U; index++) {
        sample_valid(&resolver, 0.0f, 1.0f, false, &event);
    }
    assert(pomodoro_orientation_resolver_current(&resolver) ==
           POMODORO_UI_ORIENTATION_0);

    for (unsigned index = 0; index < 6U; index++) {
        sample_valid(&resolver, 0.8f, 0.8f, false, &event);
        assert(!event.orientation_changed);
    }
    for (unsigned index = 0; index < 6U; index++) {
        sample_valid(&resolver, 0.0f, 0.0f, false, &event);
        assert(!event.orientation_changed);
    }
    assert(pomodoro_orientation_resolver_current(&resolver) ==
           POMODORO_UI_ORIENTATION_0);
}

static void test_stable_during_touch_is_deferred_until_release(void)
{
    pomodoro_orientation_resolver_t resolver;
    pomodoro_orientation_resolver_init(&resolver);
    pomodoro_orientation_event_t event;

    for (unsigned index = 0; index < 5U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, false, &event);
    }
    for (unsigned index = 0; index < 3U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, true, &event);
        assert(!event.orientation_changed);
    }
    assert(pomodoro_orientation_resolver_current(&resolver) ==
           POMODORO_UI_ORIENTATION_0);
    sample_valid(&resolver, -1.0f, 0.0f, false, &event);
    assert(event.orientation_changed);
    assert(event.orientation == POMODORO_UI_ORIENTATION_270);
    assert(pomodoro_orientation_resolver_current(&resolver) ==
           POMODORO_UI_ORIENTATION_270);
}

static void test_pending_survives_ambiguous_samples_until_release(void)
{
    pomodoro_orientation_resolver_t resolver;
    pomodoro_orientation_resolver_init(&resolver);
    pomodoro_orientation_event_t event;

    for (unsigned index = 0; index < 6U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, true, &event);
        assert(!event.orientation_changed);
    }
    assert(pomodoro_orientation_resolver_current(&resolver) ==
           POMODORO_UI_ORIENTATION_0);
    sample_valid(&resolver, 0.0f, 0.0f, true, &event);
    assert(!event.orientation_changed);
    sample_valid(&resolver, -1.0f, 0.0f, false, &event);
    assert(event.orientation_changed);
    assert(event.orientation == POMODORO_UI_ORIENTATION_270);
}

static void test_pending_is_cancelled_when_device_returns_to_current(void)
{
    pomodoro_orientation_resolver_t resolver;
    pomodoro_orientation_resolver_init(&resolver);
    pomodoro_orientation_event_t event;

    for (unsigned index = 0; index < 6U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, true, &event);
    }
    for (unsigned index = 0; index < 6U; index++) {
        sample_valid(&resolver, 0.0f, 1.0f, true, &event);
        assert(!event.orientation_changed);
    }
    sample_valid(&resolver, 0.0f, 1.0f, false, &event);
    assert(!event.orientation_changed);
    assert(pomodoro_orientation_resolver_current(&resolver) ==
           POMODORO_UI_ORIENTATION_0);
}

static void test_transient_failures_preserve_until_third(void)
{
    pomodoro_orientation_resolver_t resolver;
    pomodoro_orientation_resolver_init(&resolver);
    pomodoro_orientation_event_t event;

    sample_failure(&resolver, false, &event);
    assert(!event.warning_started);
    assert(!event.retry_sensor);
    sample_failure(&resolver, false, &event);
    assert(!event.warning_started);
    assert(pomodoro_orientation_resolver_current(&resolver) ==
           POMODORO_UI_ORIENTATION_0);
    sample_failure(&resolver, false, &event);
    assert(event.warning_started);
    assert(pomodoro_orientation_resolver_current(&resolver) ==
           POMODORO_UI_ORIENTATION_0);
    sample_failure(&resolver, false, &event);
    assert(!event.warning_started);
}

static void test_unavailable_retry_timing(void)
{
    pomodoro_orientation_resolver_t resolver;
    pomodoro_orientation_resolver_init(&resolver);
    pomodoro_orientation_event_t event;

    for (unsigned index = 0; index < 3U; index++) {
        sample_failure(&resolver, false, &event);
    }
    assert(event.warning_started);

    for (unsigned index = 0; index < 49U; index++) {
        sample_failure(&resolver, false, &event);
        assert(!event.retry_sensor);
    }
    sample_failure(&resolver, false, &event);
    assert(event.retry_sensor);
    for (unsigned index = 0; index < 49U; index++) {
        sample_failure(&resolver, false, &event);
        assert(!event.retry_sensor);
    }
    sample_failure(&resolver, false, &event);
    assert(event.retry_sensor);
}

static void test_recovery_is_silent_and_resumes_classification(void)
{
    pomodoro_orientation_resolver_t resolver;
    pomodoro_orientation_resolver_init(&resolver);
    pomodoro_orientation_event_t event;

    for (unsigned index = 0; index < 3U; index++) {
        sample_failure(&resolver, false, &event);
    }
    assert(event.warning_started);

    for (unsigned index = 0; index < 49U; index++) {
        sample_failure(&resolver, false, &event);
        assert(!event.retry_sensor);
    }
    sample_failure(&resolver, false, &event);
    assert(event.retry_sensor);
    pomodoro_orientation_resolver_report_reinit(&resolver, true);
    assert(!event.warning_started);

    for (unsigned index = 0; index < 5U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, false, &event);
        assert(!event.orientation_changed);
    }
    sample_valid(&resolver, -1.0f, 0.0f, false, &event);
    assert(event.orientation_changed);
    assert(event.orientation == POMODORO_UI_ORIENTATION_270);
}

static void test_recovery_waits_for_successful_reinit(void)
{
    pomodoro_orientation_resolver_t resolver;
    pomodoro_orientation_resolver_init(&resolver);
    pomodoro_orientation_event_t event;

    for (unsigned index = 0; index < 3U; index++) {
        sample_failure(&resolver, false, &event);
    }
    assert(event.warning_started);

    for (unsigned index = 0; index < 50U; index++) {
        sample_failure(&resolver, false, &event);
    }
    assert(event.retry_sensor);
    pomodoro_orientation_resolver_report_reinit(&resolver, false);

    for (unsigned index = 0; index < 6U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, false, &event);
        assert(!event.orientation_changed);
    }
    assert(pomodoro_orientation_resolver_current(&resolver) ==
           POMODORO_UI_ORIENTATION_0);

    for (unsigned index = 0; index < 43U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, false, &event);
        assert(!event.retry_sensor);
    }
    sample_valid(&resolver, -1.0f, 0.0f, false, &event);
    assert(event.retry_sensor);
    pomodoro_orientation_resolver_report_reinit(&resolver, true);
    for (unsigned index = 0; index < 5U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, false, &event);
    }
    sample_valid(&resolver, -1.0f, 0.0f, false, &event);
    assert(event.orientation_changed);
    assert(event.orientation == POMODORO_UI_ORIENTATION_270);
}

static void test_warning_is_once_per_boot(void)
{
    pomodoro_orientation_resolver_t resolver;
    pomodoro_orientation_resolver_init(&resolver);
    pomodoro_orientation_event_t event;

    for (unsigned index = 0; index < 3U; index++) {
        sample_failure(&resolver, false, &event);
    }
    assert(event.warning_started);
    sample_valid(&resolver, 0.0f, 1.0f, false, &event);

    for (unsigned index = 0; index < 3U; index++) {
        sample_failure(&resolver, false, &event);
    }
    assert(!event.warning_started);
}

static void test_failure_while_candidate_accumulating_preserves_direction(void)
{
    pomodoro_orientation_resolver_t resolver;
    pomodoro_orientation_resolver_init(&resolver);
    pomodoro_orientation_event_t event;

    for (unsigned index = 0; index < 5U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, false, &event);
    }
    sample_failure(&resolver, false, &event);
    assert(!event.orientation_changed);
    assert(pomodoro_orientation_resolver_current(&resolver) ==
           POMODORO_UI_ORIENTATION_0);
}

static void test_read_failure_breaks_sample_streak(void)
{
    pomodoro_orientation_resolver_t resolver;
    pomodoro_orientation_resolver_init(&resolver);
    pomodoro_orientation_event_t event;

    for (unsigned index = 0; index < 5U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, false, &event);
    }
    sample_failure(&resolver, false, &event);
    for (unsigned index = 0; index < 5U; index++) {
        sample_valid(&resolver, -1.0f, 0.0f, false, &event);
        assert(!event.orientation_changed);
    }
    sample_valid(&resolver, -1.0f, 0.0f, false, &event);
    assert(event.orientation_changed);
    assert(event.orientation == POMODORO_UI_ORIENTATION_270);
}

int main(void)
{
    test_classify_all_four_directions();
    test_validity_and_dominance_thresholds();
    test_six_consecutive_samples_rotate_once();
    test_ambiguous_sample_resets_candidate();
    test_preserve_current_direction_on_ambiguous_placement();
    test_stable_during_touch_is_deferred_until_release();
    test_pending_survives_ambiguous_samples_until_release();
    test_pending_is_cancelled_when_device_returns_to_current();
    test_transient_failures_preserve_until_third();
    test_unavailable_retry_timing();
    test_recovery_is_silent_and_resumes_classification();
    test_recovery_waits_for_successful_reinit();
    test_warning_is_once_per_boot();
    test_failure_while_candidate_accumulating_preserves_direction();
    test_read_failure_breaks_sample_streak();
    printf("pomodoro_orientation_test: all assertions passed\n");
    return 0;
}
