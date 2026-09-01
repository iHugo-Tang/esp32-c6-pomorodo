#ifndef POMODORO_AUDIO_CUE_H
#define POMODORO_AUDIO_CUE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    POMODORO_AUDIO_CUE_NONE = 0,
    POMODORO_AUDIO_CUE_WAITING,
    POMODORO_AUDIO_CUE_FOCUS_COMPLETE,
    POMODORO_AUDIO_CUE_BREAK_COMPLETE,
} pomodoro_audio_cue_t;

typedef struct {
    uint16_t frequency_hz;
    uint16_t duration_ms;
} pomodoro_audio_segment_t;

typedef struct {
    pomodoro_audio_segment_t segments[2];
    uint8_t segment_count;
    uint16_t gap_ms;
} pomodoro_audio_pattern_t;

typedef enum {
    POMODORO_AUDIO_REQUEST_REJECTED = 0,
    POMODORO_AUDIO_REQUEST_START,
    POMODORO_AUDIO_REQUEST_QUEUED,
} pomodoro_audio_request_result_t;

typedef struct {
    pomodoro_audio_cue_t active;
    pomodoro_audio_cue_t pending_completion;
} pomodoro_audio_queue_t;

bool pomodoro_audio_cue_pattern(pomodoro_audio_cue_t cue,
                                pomodoro_audio_pattern_t *pattern);
void pomodoro_audio_queue_init(pomodoro_audio_queue_t *queue);
pomodoro_audio_request_result_t pomodoro_audio_queue_request(
    pomodoro_audio_queue_t *queue, pomodoro_audio_cue_t cue);
pomodoro_audio_cue_t pomodoro_audio_queue_active(
    const pomodoro_audio_queue_t *queue);
bool pomodoro_audio_queue_finish(pomodoro_audio_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif
