#ifndef POMODORO_PLATFORM_H
#define POMODORO_PLATFORM_H

#include <stdbool.h>
#include "esp_err.h"
#include "pomodoro_persistence.h"

#ifdef __cplusplus
extern "C" {
#endif

bool pomodoro_nvs_journal_init(pomodoro_journal_t *journal);

/* Initialize the target board and start the application/UI event loop. */
esp_err_t pomodoro_platform_start(void);

#ifdef __cplusplus
}
#endif

#endif
