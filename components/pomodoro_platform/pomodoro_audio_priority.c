#include "pomodoro_audio_priority.h"

#include <stddef.h>

void pomodoro_audio_priority_init(pomodoro_audio_priority_t *priority)
{
    if (priority != NULL) {
        priority->active = POMODORO_AUDIO_CUE_NONE;
    }
}

pomodoro_audio_request_result_t pomodoro_audio_priority_request(
    pomodoro_audio_priority_t *priority, pomodoro_audio_cue_t cue,
    uint8_t volume)
{
    if (priority == NULL || cue == POMODORO_AUDIO_CUE_NONE) {
        return POMODORO_AUDIO_REQUEST_REJECTED;
    }
    if (volume == 0U) {
        return POMODORO_AUDIO_REQUEST_SILENT;
    }
    if (priority->active == POMODORO_AUDIO_CUE_NONE) {
        priority->active = cue;
        return POMODORO_AUDIO_REQUEST_START;
    }
    return POMODORO_AUDIO_REQUEST_REJECTED;
}

pomodoro_audio_cue_t pomodoro_audio_priority_active(
    const pomodoro_audio_priority_t *priority)
{
    return priority != NULL ? priority->active : POMODORO_AUDIO_CUE_NONE;
}

void pomodoro_audio_priority_finish(pomodoro_audio_priority_t *priority)
{
    if (priority != NULL) {
        priority->active = POMODORO_AUDIO_CUE_NONE;
    }
}
