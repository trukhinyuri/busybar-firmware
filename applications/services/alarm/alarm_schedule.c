/**
 * @file alarm_schedule.c
 * @brief Pure scheduling arithmetic for the alarm service.
 *
 * Everything here is a pure function of its arguments so the firing rules can be
 * reasoned about — and unit tested — without a device, a clock or any service.
 */
#include "alarm_i.h"

#define MINUTES_PER_DAY (24 * 60)
#define DAYS_PER_WEEK   (7)

/**
 * Days from the Unix epoch to a civil date, after Howard Hinnant's `days_from_civil`.
 *
 * Valid for any Gregorian date; the shift to a March-based year removes the leap
 * day special case, which is what keeps this branch-free and exact.
 */
static int alarm_days_from_civil(int year, int month, int day) {
    year -= month <= 2;

    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = (unsigned)(year - era * 400);
    const unsigned day_of_year =
        (unsigned)((153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1);
    const unsigned day_of_era =
        year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;

    return era * 146097 + (int)day_of_era - 719468;
}

int alarm_wall_minute_key(int year, int month, int day, int hour, int minute) {
    return alarm_days_from_civil(year, month, day) * MINUTES_PER_DAY + hour * 60 + minute;
}

/** Weekday of a day that is @p day_delta days away from a weekday, Monday = 1. */
static int alarm_shift_weekday(int weekday, int day_delta) {
    int shifted = (weekday - 1 + day_delta) % DAYS_PER_WEEK;
    if(shifted < 0) shifted += DAYS_PER_WEEK;

    return shifted + 1;
}

/** Whether an alarm repeats on a given weekday, Monday = 1. */
static bool alarm_repeats_on(const AlarmEntry* entry, int weekday) {
    return (entry->weekdays & (1U << (weekday - 1))) != 0;
}

bool alarm_previous_due_key(const AlarmEntry* entry, int now_key, int now_weekday, int* due_key) {
    furi_assert(entry);
    furi_assert(due_key);

    const int today_start = (now_key / MINUTES_PER_DAY) * MINUTES_PER_DAY;
    const int alarm_minute_of_day = entry->hour * 60 + entry->minute;

    /*
     * A one-shot alarm has no weekday mask, so the only candidate that can be
     * "due" is today's — an older one would mean the alarm already fired and
     * disabled itself.
     */
    const int search_days = (entry->weekdays == AlarmWeekdayNone) ? 1 : DAYS_PER_WEEK;

    for(int days_back = 0; days_back < search_days; days_back++) {
        const int candidate = today_start - days_back * MINUTES_PER_DAY + alarm_minute_of_day;
        if(candidate > now_key) continue;

        if(entry->weekdays != AlarmWeekdayNone) {
            if(!alarm_repeats_on(entry, alarm_shift_weekday(now_weekday, -days_back))) continue;
        }

        *due_key = candidate;
        return true;
    }

    return false;
}

bool alarm_next_due_key(const AlarmEntry* entry, int now_key, int now_weekday, int* due_key) {
    furi_assert(entry);
    furi_assert(due_key);

    const int today_start = (now_key / MINUTES_PER_DAY) * MINUTES_PER_DAY;
    const int alarm_minute_of_day = entry->hour * 60 + entry->minute;

    /* A one-shot alarm may still be due today; otherwise it rolls to tomorrow. */
    const int search_days = (entry->weekdays == AlarmWeekdayNone) ? 2 : DAYS_PER_WEEK + 1;

    for(int days_ahead = 0; days_ahead < search_days; days_ahead++) {
        const int candidate = today_start + days_ahead * MINUTES_PER_DAY + alarm_minute_of_day;
        if(candidate <= now_key) continue;

        if(entry->weekdays != AlarmWeekdayNone) {
            if(!alarm_repeats_on(entry, alarm_shift_weekday(now_weekday, days_ahead))) continue;
        }

        *due_key = candidate;
        return true;
    }

    return false;
}

void alarm_entry_from_settings(const AlarmSettingsEntryV1* source, AlarmEntry* destination) {
    furi_assert(source);
    furi_assert(destination);

    destination->enabled = source->enabled;
    destination->hour = (uint8_t)source->hour;
    destination->minute = (uint8_t)source->minute;
    destination->weekdays = (uint8_t)source->weekdays;
    destination->volume = (uint8_t)source->volume;
}

void alarm_entry_to_settings(const AlarmEntry* source, AlarmSettingsEntryV1* destination) {
    furi_assert(source);
    furi_assert(destination);

    destination->enabled = source->enabled;
    destination->hour = source->hour;
    destination->minute = source->minute;
    destination->weekdays = source->weekdays;
    destination->volume = source->volume;
    /* last_handled is bookkeeping owned by the service and deliberately preserved. */
}
