#include "pomodoro_audio_cue.h"

#include <stddef.h>
#include <string.h>

static bool cue_is_completion(pomodoro_audio_cue_t cue)
{
    return cue == POMODORO_AUDIO_CUE_FOCUS_COMPLETE ||
        cue == POMODORO_AUDIO_CUE_BREAK_COMPLETE;
}

bool pomodoro_audio_cue_pattern(pomodoro_audio_cue_t cue,
                                pomodoro_audio_pattern_t *pattern)
{
    if (pattern == NULL) {
        return false;
    }
    memset(pattern, 0, sizeof(*pattern));
    switch (cue) {
    case POMODORO_AUDIO_CUE_WAITING:
        pattern->segments[0] = (pomodoro_audio_segment_t) {
            .frequency_hz = 740U,
            .duration_ms = 120U,
        };
        pattern->segment_count = 1U;
        return true;
    case POMODORO_AUDIO_CUE_FOCUS_COMPLETE:
        pattern->segments[0] = (pomodoro_audio_segment_t) {
            .frequency_hz = 660U,
            .duration_ms = 120U,
        };
        pattern->segments[1] = (pomodoro_audio_segment_t) {
            .frequency_hz = 880U,
            .duration_ms = 120U,
        };
        pattern->segment_count = 2U;
        pattern->gap_ms = 60U;
        return true;
    case POMODORO_AUDIO_CUE_BREAK_COMPLETE:
        pattern->segments[0] = (pomodoro_audio_segment_t) {
            .frequency_hz = 523U,
            .duration_ms = 180U,
        };
        pattern->segment_count = 1U;
        return true;
    case POMODORO_AUDIO_CUE_NONE:
    default:
        return false;
    }
}

void pomodoro_audio_queue_init(pomodoro_audio_queue_t *queue)
{
    if (queue != NULL) {
        queue->active = POMODORO_AUDIO_CUE_NONE;
        queue->pending_completion = POMODORO_AUDIO_CUE_NONE;
    }
}

pomodoro_audio_request_result_t pomodoro_audio_queue_request(
    pomodoro_audio_queue_t *queue, pomodoro_audio_cue_t cue)
{
    if (queue == NULL || cue == POMODORO_AUDIO_CUE_NONE) {
        return POMODORO_AUDIO_REQUEST_REJECTED;
    }
    if (queue->active == POMODORO_AUDIO_CUE_NONE) {
        queue->active = cue;
        return POMODORO_AUDIO_REQUEST_START;
    }
    if (cue_is_completion(cue) &&
        queue->active == POMODORO_AUDIO_CUE_WAITING &&
        queue->pending_completion == POMODORO_AUDIO_CUE_NONE) {
        queue->pending_completion = cue;
        return POMODORO_AUDIO_REQUEST_QUEUED;
    }
    return POMODORO_AUDIO_REQUEST_REJECTED;
}

pomodoro_audio_cue_t pomodoro_audio_queue_active(
    const pomodoro_audio_queue_t *queue)
{
    return queue != NULL ? queue->active : POMODORO_AUDIO_CUE_NONE;
}

bool pomodoro_audio_queue_finish(pomodoro_audio_queue_t *queue)
{
    if (queue == NULL || queue->active == POMODORO_AUDIO_CUE_NONE) {
        return false;
    }
    queue->active = queue->pending_completion;
    queue->pending_completion = POMODORO_AUDIO_CUE_NONE;
    return queue->active != POMODORO_AUDIO_CUE_NONE;
}
