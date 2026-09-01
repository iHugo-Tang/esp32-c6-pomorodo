#include "pomodoro_audio_priority.h"

#include <assert.h>
#include <stdio.h>

static void test_fixed_tone_parameters_match_product_feedback(void)
{
    assert(POMODORO_WAITING_TONE_FREQUENCY_HZ == 740U);
    assert(POMODORO_WAITING_TONE_DURATION_MS == 120U);
    assert(POMODORO_BREAK_TONE_FREQUENCY_HZ == 523U);
    assert(POMODORO_BREAK_TONE_DURATION_MS == 180U);
    assert(POMODORO_FOCUS_TONE_FIRST_FREQUENCY_HZ == 660U);
    assert(POMODORO_FOCUS_TONE_SECOND_FREQUENCY_HZ == 880U);
    assert(POMODORO_FOCUS_TONE_DURATION_MS == 120U);
    assert(POMODORO_FOCUS_TONE_GAP_MS == 60U);
}

static void test_zero_volume_succeeds_without_occupying_audio(void)
{
    pomodoro_audio_priority_t priority;
    pomodoro_audio_priority_init(&priority);

    assert(pomodoro_audio_priority_request(
               &priority, POMODORO_AUDIO_CUE_WAITING, 0U) ==
           POMODORO_AUDIO_REQUEST_SILENT);
    assert(pomodoro_audio_priority_active(&priority) ==
           POMODORO_AUDIO_CUE_NONE);
}

static void test_completion_queues_behind_waiting_but_waiting_never_queues(void)
{
    pomodoro_audio_priority_t priority;
    pomodoro_audio_priority_init(&priority);

    assert(pomodoro_audio_priority_request(
               &priority, POMODORO_AUDIO_CUE_WAITING, 3U) ==
           POMODORO_AUDIO_REQUEST_START);
    assert(pomodoro_audio_priority_request(
               &priority, POMODORO_AUDIO_CUE_WAITING, 3U) ==
           POMODORO_AUDIO_REQUEST_REJECTED);
    assert(pomodoro_audio_priority_request(
               &priority, POMODORO_AUDIO_CUE_FOCUS_COMPLETE, 3U) ==
           POMODORO_AUDIO_REQUEST_QUEUED);
    assert(pomodoro_audio_priority_request(
               &priority, POMODORO_AUDIO_CUE_BREAK_COMPLETE, 3U) ==
           POMODORO_AUDIO_REQUEST_REJECTED);

    assert(pomodoro_audio_priority_finish(&priority));
    assert(pomodoro_audio_priority_active(&priority) ==
           POMODORO_AUDIO_CUE_FOCUS_COMPLETE);
    assert(!pomodoro_audio_priority_finish(&priority));
    assert(pomodoro_audio_priority_active(&priority) ==
           POMODORO_AUDIO_CUE_NONE);
}

static void test_busy_completion_rejects_every_other_cue(void)
{
    pomodoro_audio_priority_t priority;
    pomodoro_audio_priority_init(&priority);

    assert(pomodoro_audio_priority_request(
               &priority, POMODORO_AUDIO_CUE_BREAK_COMPLETE, 3U) ==
           POMODORO_AUDIO_REQUEST_START);
    assert(pomodoro_audio_priority_request(
               &priority, POMODORO_AUDIO_CUE_WAITING, 3U) ==
           POMODORO_AUDIO_REQUEST_REJECTED);
    assert(pomodoro_audio_priority_request(
               &priority, POMODORO_AUDIO_CUE_FOCUS_COMPLETE, 3U) ==
           POMODORO_AUDIO_REQUEST_REJECTED);
}

int main(void)
{
    test_fixed_tone_parameters_match_product_feedback();
    test_zero_volume_succeeds_without_occupying_audio();
    test_completion_queues_behind_waiting_but_waiting_never_queues();
    test_busy_completion_rejects_every_other_cue();
    puts("pomodoro_audio_priority_test: all tests passed");
    return 0;
}
