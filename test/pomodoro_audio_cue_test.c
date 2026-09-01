#include "pomodoro_audio_cue.h"

#include <assert.h>
#include <stdio.h>

static void test_cue_patterns_are_distinct(void)
{
    pomodoro_audio_pattern_t pattern;

    assert(pomodoro_audio_cue_pattern(POMODORO_AUDIO_CUE_WAITING, &pattern));
    assert(pattern.segment_count == 1U);
    assert(pattern.segments[0].frequency_hz == 740U);
    assert(pattern.segments[0].duration_ms == 120U);
    assert(pattern.gap_ms == 0U);

    assert(pomodoro_audio_cue_pattern(POMODORO_AUDIO_CUE_BREAK_COMPLETE,
                                      &pattern));
    assert(pattern.segment_count == 1U);
    assert(pattern.segments[0].frequency_hz == 523U);
    assert(pattern.segments[0].duration_ms == 180U);

    assert(pomodoro_audio_cue_pattern(POMODORO_AUDIO_CUE_FOCUS_COMPLETE,
                                      &pattern));
    assert(pattern.segment_count == 2U);
    assert(pattern.segments[0].frequency_hz == 660U);
    assert(pattern.segments[0].duration_ms == 120U);
    assert(pattern.segments[1].frequency_hz == 880U);
    assert(pattern.segments[1].duration_ms == 120U);
    assert(pattern.gap_ms == 60U);
}

static void test_completion_queues_behind_waiting_but_waiting_never_queues(void)
{
    pomodoro_audio_queue_t queue;
    pomodoro_audio_queue_init(&queue);

    assert(pomodoro_audio_queue_request(
               &queue, POMODORO_AUDIO_CUE_WAITING) ==
           POMODORO_AUDIO_REQUEST_START);
    assert(pomodoro_audio_queue_request(
               &queue, POMODORO_AUDIO_CUE_WAITING) ==
           POMODORO_AUDIO_REQUEST_REJECTED);
    assert(pomodoro_audio_queue_request(
               &queue, POMODORO_AUDIO_CUE_FOCUS_COMPLETE) ==
           POMODORO_AUDIO_REQUEST_QUEUED);
    assert(pomodoro_audio_queue_request(
               &queue, POMODORO_AUDIO_CUE_BREAK_COMPLETE) ==
           POMODORO_AUDIO_REQUEST_REJECTED);

    assert(pomodoro_audio_queue_finish(&queue));
    assert(pomodoro_audio_queue_active(&queue) ==
           POMODORO_AUDIO_CUE_FOCUS_COMPLETE);
    assert(!pomodoro_audio_queue_finish(&queue));
    assert(pomodoro_audio_queue_active(&queue) == POMODORO_AUDIO_CUE_NONE);
}

static void test_busy_completion_rejects_every_other_cue(void)
{
    pomodoro_audio_queue_t queue;
    pomodoro_audio_queue_init(&queue);

    assert(pomodoro_audio_queue_request(
               &queue, POMODORO_AUDIO_CUE_BREAK_COMPLETE) ==
           POMODORO_AUDIO_REQUEST_START);
    assert(pomodoro_audio_queue_request(
               &queue, POMODORO_AUDIO_CUE_WAITING) ==
           POMODORO_AUDIO_REQUEST_REJECTED);
    assert(pomodoro_audio_queue_request(
               &queue, POMODORO_AUDIO_CUE_FOCUS_COMPLETE) ==
           POMODORO_AUDIO_REQUEST_REJECTED);
}

int main(void)
{
    test_cue_patterns_are_distinct();
    test_completion_queues_behind_waiting_but_waiting_never_queues();
    test_busy_completion_rejects_every_other_cue();
    puts("pomodoro_audio_cue_test: all tests passed");
    return 0;
}
