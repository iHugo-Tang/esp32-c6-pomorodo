#ifndef POMODORO_AUDIO_PRIORITY_H
#define POMODORO_AUDIO_PRIORITY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POMODORO_WAITING_TONE_FREQUENCY_HZ 740U
#define POMODORO_WAITING_TONE_DURATION_MS 120U
#define POMODORO_FOCUS_TONE_FIRST_FREQUENCY_HZ 660U
#define POMODORO_FOCUS_TONE_SECOND_FREQUENCY_HZ 880U
#define POMODORO_FOCUS_TONE_DURATION_MS 120U
#define POMODORO_FOCUS_TONE_GAP_MS 60U
#define POMODORO_BREAK_TONE_FREQUENCY_HZ 523U
#define POMODORO_BREAK_TONE_DURATION_MS 180U

typedef enum {
    POMODORO_AUDIO_CUE_NONE = 0,
    POMODORO_AUDIO_CUE_WAITING,
    POMODORO_AUDIO_CUE_FOCUS_COMPLETE,
    POMODORO_AUDIO_CUE_BREAK_COMPLETE,
} pomodoro_audio_cue_t;

typedef enum {
    POMODORO_AUDIO_REQUEST_REJECTED = 0,
    POMODORO_AUDIO_REQUEST_SILENT,
    POMODORO_AUDIO_REQUEST_START,
    POMODORO_AUDIO_REQUEST_QUEUED,
} pomodoro_audio_request_result_t;

typedef struct {
    pomodoro_audio_cue_t active;
    pomodoro_audio_cue_t pending_completion;
} pomodoro_audio_priority_t;

void pomodoro_audio_priority_init(pomodoro_audio_priority_t *priority);
pomodoro_audio_request_result_t pomodoro_audio_priority_request(
    pomodoro_audio_priority_t *priority, pomodoro_audio_cue_t cue,
    uint8_t volume);
pomodoro_audio_cue_t pomodoro_audio_priority_active(
    const pomodoro_audio_priority_t *priority);
bool pomodoro_audio_priority_finish(pomodoro_audio_priority_t *priority);

#ifdef __cplusplus
}
#endif

#endif
