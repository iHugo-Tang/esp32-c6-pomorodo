#include "pomodoro_platform.h"

#include <stddef.h>

#include "nvs.h"
#include "nvs_flash.h"

typedef struct {
    nvs_handle_t handle;
} pomodoro_nvs_context_t;

static pomodoro_nvs_context_t context;

static const char *slot_key(uint8_t slot)
{
    return slot == 0U ? "state_a" : "state_b";
}

static bool read_slot(void *opaque, uint8_t slot,
                      pomodoro_journal_slot_t *value)
{
    if (opaque == NULL || value == NULL || slot > 1U) {
        return false;
    }
    pomodoro_nvs_context_t *nvs_context = opaque;
    size_t size = sizeof(*value);
    return nvs_get_blob(nvs_context->handle, slot_key(slot), value, &size) ==
               ESP_OK &&
           size == sizeof(*value);
}

static bool write_slot(void *opaque, uint8_t slot,
                       const pomodoro_journal_slot_t *value)
{
    if (opaque == NULL || value == NULL || slot > 1U) {
        return false;
    }
    pomodoro_nvs_context_t *nvs_context = opaque;
    return nvs_set_blob(nvs_context->handle, slot_key(slot), value,
                        sizeof(*value)) == ESP_OK &&
           nvs_commit(nvs_context->handle) == ESP_OK;
}

bool pomodoro_nvs_journal_init(pomodoro_journal_t *journal)
{
    if (journal == NULL) {
        return false;
    }
    if (nvs_flash_init_partition("pomodoro") != ESP_OK) {
        return false;
    }
    if (nvs_open_from_partition("pomodoro", "state", NVS_READWRITE,
                                &context.handle) != ESP_OK) {
        return false;
    }
    journal->io = (pomodoro_journal_io_t) {
        .context = &context,
        .read_slot = read_slot,
        .write_slot = write_slot,
    };
    return true;
}
