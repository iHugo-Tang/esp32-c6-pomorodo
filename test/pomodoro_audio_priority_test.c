#include "pomodoro_audio_priority.h"

#include <assert.h>
#include <stdio.h>

static void test_zero_volume_succeeds_without_occupying_audio(void)
{
    pomodoro_audio_priority_t priority;
    pomodoro_audio_priority_init(&priority);

    assert(pomodoro_audio_priority_request(
               &priority, POMODORO_AUDIO_CUE_FOCUS_COMPLETE, 0U) ==
           POMODORO_AUDIO_REQUEST_SILENT);
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
               &priority, POMODORO_AUDIO_CUE_FOCUS_COMPLETE, 3U) ==
           POMODORO_AUDIO_REQUEST_REJECTED);
    pomodoro_audio_priority_finish(&priority);
    assert(pomodoro_audio_priority_active(&priority) ==
           POMODORO_AUDIO_CUE_NONE);
    assert(pomodoro_audio_priority_request(
               &priority, POMODORO_AUDIO_CUE_FOCUS_COMPLETE, 3U) ==
           POMODORO_AUDIO_REQUEST_START);
}

int main(void)
{
    test_zero_volume_succeeds_without_occupying_audio();
    test_busy_completion_rejects_every_other_cue();
    puts("pomodoro_audio_priority_test: all tests passed");
    return 0;
}
