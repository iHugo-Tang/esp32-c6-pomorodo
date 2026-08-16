#ifndef POMODORO_TEST_SUPPORT_H
#define POMODORO_TEST_SUPPORT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "pomodoro_persistence.h"

typedef struct {
    pomodoro_journal_slot_t slots[2];
    bool present[2];
    bool tear_next_write;
    bool fail_reads;
} pomodoro_test_memory_journal_t;

static pomodoro_journal_read_result_t pomodoro_test_memory_read_slot(
    void *context, uint8_t slot, pomodoro_journal_slot_t *value)
{
    pomodoro_test_memory_journal_t *memory = context;
    if (memory->fail_reads) {
        return POMODORO_JOURNAL_READ_ERROR;
    }
    if (slot > 1U || !memory->present[slot]) {
        return POMODORO_JOURNAL_READ_EMPTY;
    }
    *value = memory->slots[slot];
    return POMODORO_JOURNAL_READ_LOADED;
}

static bool pomodoro_test_memory_write_slot(
    void *context, uint8_t slot, const pomodoro_journal_slot_t *value)
{
    pomodoro_test_memory_journal_t *memory = context;
    if (slot > 1U) {
        return false;
    }
    memory->present[slot] = true;
    if (memory->tear_next_write) {
        memory->tear_next_write = false;
        memset(&memory->slots[slot], 0, sizeof(memory->slots[slot]));
        memcpy(&memory->slots[slot], value, sizeof(*value) / 2U);
        return true;
    }
    memory->slots[slot] = *value;
    return true;
}

static void pomodoro_test_memory_journal_init(
    pomodoro_test_memory_journal_t *memory, pomodoro_journal_t *journal)
{
    journal->io = (pomodoro_journal_io_t) {
        .context = memory,
        .read_slot = pomodoro_test_memory_read_slot,
        .write_slot = pomodoro_test_memory_write_slot,
    };
}

static uint32_t pomodoro_test_crc32(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    for (size_t index = 0; index < size; ++index) {
        crc ^= bytes[index];
        for (unsigned bit = 0; bit < 8U; ++bit) {
            uint32_t mask = (uint32_t) -(int32_t) (crc & 1U);
            crc = (crc >> 1U) ^ (UINT32_C(0xEDB88320) & mask);
        }
    }
    return ~crc;
}

#endif
