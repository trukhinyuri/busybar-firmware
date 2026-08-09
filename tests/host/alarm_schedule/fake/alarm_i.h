/**
 * @file alarm_i.h
 * @brief Host-test stand-in for the firmware header.
 *
 * `alarm_schedule.c` is deliberately free of firmware dependencies so its rules
 * can be tested on a workstation. Placing this file on the include path ahead of
 * the real one supplies just the declarations it needs, without dragging in
 * furi, the GUI or any service.
 */
#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define furi_assert(expression) assert(expression)

#define ALARM_MAX_COUNT (5U)

typedef enum {
    AlarmWeekdayMonday = (1U << 0),
    AlarmWeekdayTuesday = (1U << 1),
    AlarmWeekdayWednesday = (1U << 2),
    AlarmWeekdayThursday = (1U << 3),
    AlarmWeekdayFriday = (1U << 4),
    AlarmWeekdaySaturday = (1U << 5),
    AlarmWeekdaySunday = (1U << 6),

    AlarmWeekdayNone = 0,
    AlarmWeekdayWorkdays = 0x1F,
    AlarmWeekdayWeekend = 0x60,
    AlarmWeekdayEveryDay = 0x7F,
} AlarmWeekday;

typedef struct {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t weekdays;
    uint8_t volume;
} AlarmEntry;

typedef struct {
    bool enabled;
    int hour;
    int minute;
    int weekdays;
    int volume;
    int last_handled;
} AlarmSettingsEntryV1;

int alarm_wall_minute_key(int year, int month, int day, int hour, int minute);

bool alarm_previous_due_key(const AlarmEntry* entry, int now_key, int now_weekday, int* due_key);

bool alarm_next_due_key(const AlarmEntry* entry, int now_key, int now_weekday, int* due_key);

void alarm_entry_from_settings(const AlarmSettingsEntryV1* source, AlarmEntry* destination);

void alarm_entry_to_settings(const AlarmEntry* source, AlarmSettingsEntryV1* destination);
