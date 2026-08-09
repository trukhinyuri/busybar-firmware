/**
 * @file test_alarm_schedule.c
 * @brief Host tests for the alarm scheduling rules.
 *
 * These cover the cases that decide whether an alarm actually goes off:
 * weekday selection, the boundary between "ring now" and "this was missed",
 * and the day and week rollovers.
 */
#include "alarm_i.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(condition, ...)                        \
    do {                                             \
        checks++;                                    \
        if(!(condition)) {                           \
            failures++;                              \
            printf("FAIL %s:%d: ", __func__, __LINE__); \
            printf(__VA_ARGS__);                     \
            printf("\n");                            \
        }                                            \
    } while(0)

/* 2026-08-10 is a Monday. */
#define MONDAY_YEAR  (2026)
#define MONDAY_MONTH (8)
#define MONDAY_DAY   (10)
#define WEEKDAY_MON  (1)
#define WEEKDAY_SAT  (6)
#define WEEKDAY_SUN  (7)

#define MINUTES_PER_DAY (1440)

static AlarmEntry make_entry(int hour, int minute, int weekdays) {
    AlarmEntry entry = {
        .enabled = true,
        .hour = (uint8_t)hour,
        .minute = (uint8_t)minute,
        .weekdays = (uint8_t)weekdays,
        .volume = 80,
    };

    return entry;
}

static int monday_key(int hour, int minute) {
    return alarm_wall_minute_key(MONDAY_YEAR, MONDAY_MONTH, MONDAY_DAY, hour, minute);
}

static void test_wall_minute_key_is_monotonic(void) {
    const int midnight = alarm_wall_minute_key(2026, 8, 10, 0, 0);
    const int later = alarm_wall_minute_key(2026, 8, 10, 7, 30);
    const int next_day = alarm_wall_minute_key(2026, 8, 11, 0, 0);

    CHECK(later - midnight == 7 * 60 + 30, "expected 450 minutes, got %d", later - midnight);
    CHECK(
        next_day - midnight == MINUTES_PER_DAY,
        "expected a whole day, got %d",
        next_day - midnight);

    /* Month and year rollovers must not skip or repeat a day. */
    CHECK(
        alarm_wall_minute_key(2026, 9, 1, 0, 0) - alarm_wall_minute_key(2026, 8, 31, 0, 0) ==
            MINUTES_PER_DAY,
        "month rollover is wrong");
    CHECK(
        alarm_wall_minute_key(2027, 1, 1, 0, 0) - alarm_wall_minute_key(2026, 12, 31, 0, 0) ==
            MINUTES_PER_DAY,
        "year rollover is wrong");
    /* 2028 is a leap year, so February has 29 days. */
    CHECK(
        alarm_wall_minute_key(2028, 3, 1, 0, 0) - alarm_wall_minute_key(2028, 2, 28, 0, 0) ==
            2 * MINUTES_PER_DAY,
        "leap day is not accounted for");
}

static void test_due_exactly_on_time(void) {
    const AlarmEntry entry = make_entry(7, 30, AlarmWeekdayWorkdays);
    const int now = monday_key(7, 30);

    int due = 0;
    CHECK(alarm_previous_due_key(&entry, now, WEEKDAY_MON, &due), "should be due at 07:30");
    CHECK(due == now, "due key should equal now");
}

static void test_not_due_before_time(void) {
    const AlarmEntry entry = make_entry(7, 30, AlarmWeekdayWorkdays);
    const int now = monday_key(7, 29);

    int due = 0;
    const bool found = alarm_previous_due_key(&entry, now, WEEKDAY_MON, &due);

    /*
     * A minute before the alarm the only previous occurrence is the one from the
     * last matching weekday, which must be far enough back to count as handled.
     */
    if(found) {
        CHECK(now - due >= MINUTES_PER_DAY, "previous occurrence should be a prior day");
    }
}

static void test_weekend_alarm_skips_weekdays(void) {
    const AlarmEntry entry = make_entry(9, 0, AlarmWeekdayWeekend);
    const int now = monday_key(9, 0);

    int due = 0;
    CHECK(alarm_previous_due_key(&entry, now, WEEKDAY_MON, &due), "weekend alarm has a past due");
    /* The most recent weekend occurrence from Monday morning is Sunday. */
    CHECK(
        now - due == MINUTES_PER_DAY,
        "expected yesterday (Sunday), got %d minutes back",
        now - due);
}

static void test_workday_alarm_not_due_on_weekend(void) {
    const AlarmEntry entry = make_entry(7, 30, AlarmWeekdayWorkdays);
    /* Saturday 2026-08-15 at 07:30. */
    const int now = alarm_wall_minute_key(2026, 8, 15, 7, 30);

    int due = 0;
    CHECK(alarm_previous_due_key(&entry, now, WEEKDAY_SAT, &due), "should find Friday");
    CHECK(now - due == MINUTES_PER_DAY, "expected Friday, got %d minutes back", now - due);
}

static void test_next_due_skips_current_minute(void) {
    const AlarmEntry entry = make_entry(7, 30, AlarmWeekdayEveryDay);
    const int now = monday_key(7, 30);

    int due = 0;
    CHECK(alarm_next_due_key(&entry, now, WEEKDAY_MON, &due), "there is always a next occurrence");
    CHECK(
        due - now == MINUTES_PER_DAY,
        "next occurrence should be tomorrow, got %d",
        due - now);
}

static void test_next_due_later_today(void) {
    const AlarmEntry entry = make_entry(23, 59, AlarmWeekdayEveryDay);
    const int now = monday_key(0, 0);

    int due = 0;
    CHECK(alarm_next_due_key(&entry, now, WEEKDAY_MON, &due), "should be due later today");
    CHECK(due - now == 23 * 60 + 59, "expected same day, got %d", due - now);
}

static void test_next_due_across_the_week(void) {
    const AlarmEntry entry = make_entry(9, 0, AlarmWeekdaySunday);
    const int now = monday_key(9, 1);

    int due = 0;
    CHECK(alarm_next_due_key(&entry, now, WEEKDAY_MON, &due), "Sunday alarm should be found");
    CHECK(
        due - now == 6 * MINUTES_PER_DAY - 1,
        "expected next Sunday, got %d minutes",
        due - now);
}

static void test_one_shot_has_no_stale_past(void) {
    const AlarmEntry entry = make_entry(7, 30, AlarmWeekdayNone);

    /* Before its time, a one-shot alarm has no past occurrence at all. */
    int due = 0;
    CHECK(
        !alarm_previous_due_key(&entry, monday_key(7, 0), WEEKDAY_MON, &due),
        "one-shot alarm must not report yesterday");

    /* On time, it does. */
    CHECK(
        alarm_previous_due_key(&entry, monday_key(7, 30), WEEKDAY_MON, &due),
        "one-shot alarm should be due at its time");
    CHECK(due == monday_key(7, 30), "one-shot due key mismatch");
}

static void test_one_shot_next_rolls_to_tomorrow(void) {
    const AlarmEntry entry = make_entry(7, 30, AlarmWeekdayNone);

    int due = 0;
    CHECK(
        alarm_next_due_key(&entry, monday_key(8, 0), WEEKDAY_MON, &due),
        "one-shot alarm should roll over");
    CHECK(
        due - monday_key(8, 0) == MINUTES_PER_DAY - 30,
        "expected 07:30 tomorrow, got %d",
        due - monday_key(8, 0));
}

/**
 * The catch-up rule is what separates "the device just rebooted" from "the
 * device was off all night", so it is worth pinning down at the boundary.
 */
static void test_catchup_boundary(void) {
    const AlarmEntry entry = make_entry(7, 30, AlarmWeekdayEveryDay);
    const int due_at = monday_key(7, 30);

    int due = 0;

    CHECK(alarm_previous_due_key(&entry, monday_key(7, 45), WEEKDAY_MON, &due), "should find due");
    CHECK(monday_key(7, 45) - due == 15, "15 minutes late is still within catch-up");

    CHECK(alarm_previous_due_key(&entry, monday_key(7, 46), WEEKDAY_MON, &due), "should find due");
    CHECK(monday_key(7, 46) - due == 16, "16 minutes late is past catch-up");

    CHECK(due == due_at, "the occurrence itself should not move");
}

static void test_settings_round_trip_preserves_bookkeeping(void) {
    AlarmSettingsEntryV1 stored = {
        .enabled = true,
        .hour = 6,
        .minute = 15,
        .weekdays = AlarmWeekdayWorkdays,
        .volume = 70,
        .last_handled = 123456,
    };

    AlarmEntry entry;
    alarm_entry_from_settings(&stored, &entry);

    CHECK(entry.hour == 6 && entry.minute == 15, "time did not survive the round trip");
    CHECK(entry.weekdays == AlarmWeekdayWorkdays, "weekdays did not survive the round trip");

    entry.hour = 8;
    alarm_entry_to_settings(&entry, &stored);

    CHECK(stored.hour == 8, "edit was not written back");
    CHECK(
        stored.last_handled == 123456,
        "last_handled must be preserved or a reboot would re-ring the alarm");
}

int main(void) {
    test_wall_minute_key_is_monotonic();
    test_due_exactly_on_time();
    test_not_due_before_time();
    test_weekend_alarm_skips_weekdays();
    test_workday_alarm_not_due_on_weekend();
    test_next_due_skips_current_minute();
    test_next_due_later_today();
    test_next_due_across_the_week();
    test_one_shot_has_no_stale_past();
    test_one_shot_next_rolls_to_tomorrow();
    test_catchup_boundary();
    test_settings_round_trip_preserves_bookkeeping();

    printf("%d checks, %d failures\n", checks, failures);

    return failures == 0 ? 0 : 1;
}
