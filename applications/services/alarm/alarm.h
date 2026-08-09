/**
 * @file alarm.h
 * @brief Autonomous alarm clock service API.
 *
 * The service owns the alarm schedule and rings independently of the foreground
 * application, the mode selector position, network availability and display
 * sleep state. Applications use this API to inspect and edit the schedule; they
 * never perform scheduling themselves.
 */
#pragma once

#include <furi.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Record name for the Alarm service */
#define RECORD_ALARM "alarm"

/** Maximum number of alarms that can be stored */
#define ALARM_MAX_COUNT (5U)

/** Lowest selectable alarm volume, in percent */
#define ALARM_VOLUME_MIN (10U)

/** Highest selectable alarm volume, in percent */
#define ALARM_VOLUME_MAX (100U)

/** Weekday bit positions for AlarmEntry::weekdays (Monday = bit 0) */
typedef enum {
    AlarmWeekdayMonday = (1U << 0),
    AlarmWeekdayTuesday = (1U << 1),
    AlarmWeekdayWednesday = (1U << 2),
    AlarmWeekdayThursday = (1U << 3),
    AlarmWeekdayFriday = (1U << 4),
    AlarmWeekdaySaturday = (1U << 5),
    AlarmWeekdaySunday = (1U << 6),

    AlarmWeekdayNone = 0, /**< No repeat: the alarm fires once, then disables itself */
    AlarmWeekdayWorkdays = 0x1F, /**< Monday to Friday */
    AlarmWeekdayWeekend = 0x60, /**< Saturday and Sunday */
    AlarmWeekdayEveryDay = 0x7F, /**< All seven days */
} AlarmWeekday;

/** A single alarm definition. */
typedef struct {
    bool enabled; /**< Whether the alarm is armed */
    uint8_t hour; /**< Local wall-clock hour, 0-23 */
    uint8_t minute; /**< Local wall-clock minute, 0-59 */
    uint8_t weekdays; /**< Bitmask of AlarmWeekday, or AlarmWeekdayNone for one-shot */
    uint8_t volume; /**< Ringing volume in percent, ALARM_VOLUME_MIN..ALARM_VOLUME_MAX */
} AlarmEntry;

/** Kinds of events published on the Alarm pubsub. */
typedef enum {
    AlarmEventScheduleChanged, /**< An alarm was added, edited or removed */
    AlarmEventStarted, /**< An alarm started ringing */
    AlarmEventStopped, /**< A ringing alarm was dismissed or timed out */
} AlarmEventType;

/** Event published on the Alarm pubsub. */
typedef struct {
    AlarmEventType type;
} AlarmEvent;

/** Alarm service opaque type declaration */
typedef struct Alarm Alarm;

/**
 * @brief Get the pubsub used to report schedule and ringing changes.
 *
 * @param[in] instance pointer to the Alarm service instance
 * @return pointer to the FuriPubSub carrying AlarmEvent values
 */
FuriPubSub* alarm_get_pubsub(Alarm* instance);

/**
 * @brief Get the number of stored alarms.
 *
 * @param[in] instance pointer to the Alarm service instance
 * @return stored alarm count, 0..ALARM_MAX_COUNT
 */
size_t alarm_get_count(Alarm* instance);

/**
 * @brief Read a stored alarm.
 *
 * @param[in] instance pointer to the Alarm service instance
 * @param[in] index alarm index
 * @param[out] entry destination for the alarm definition
 * @return true if the index was valid and @p entry was filled
 */
bool alarm_get_entry(Alarm* instance, size_t index, AlarmEntry* entry);

/**
 * @brief Replace a stored alarm.
 *
 * The schedule is re-armed and persisted before this call returns.
 *
 * @param[in,out] instance pointer to the Alarm service instance
 * @param[in] index alarm index
 * @param[in] entry new alarm definition
 * @return true if the index was valid and the alarm was stored
 */
bool alarm_set_entry(Alarm* instance, size_t index, const AlarmEntry* entry);

/**
 * @brief Append a new alarm.
 *
 * @param[in,out] instance pointer to the Alarm service instance
 * @param[in] entry alarm definition to append
 * @return true if there was room and the alarm was stored
 */
bool alarm_add_entry(Alarm* instance, const AlarmEntry* entry);

/**
 * @brief Remove a stored alarm.
 *
 * @param[in,out] instance pointer to the Alarm service instance
 * @param[in] index alarm index
 * @return true if the index was valid and the alarm was removed
 */
bool alarm_remove_entry(Alarm* instance, size_t index);

/**
 * @brief Get a default alarm definition suitable for a newly created alarm.
 *
 * @param[out] entry destination for the default definition
 */
void alarm_get_default_entry(AlarmEntry* entry);

/**
 * @brief Check whether an alarm is currently ringing.
 *
 * @param[in] instance pointer to the Alarm service instance
 * @return true if an alarm is ringing
 */
bool alarm_is_ringing(Alarm* instance);

/**
 * @brief Describe an alarm's repeat mask in a human readable form.
 *
 * Produces strings such as `Once`, `Every day`, `Workdays` or `Mon Wed Fri`.
 *
 * @param[in] weekdays bitmask of AlarmWeekday
 * @param[out] buffer destination buffer
 * @param[in] buffer_size size of @p buffer in bytes, 32 or more recommended
 */
void alarm_format_weekdays(uint8_t weekdays, char* buffer, size_t buffer_size);

/**
 * @brief Get the number of minutes until an alarm next rings.
 *
 * @param[in] instance pointer to the Alarm service instance
 * @param[out] minutes destination for the wait time in minutes
 * @return true if at least one alarm is armed and @p minutes was filled
 */
bool alarm_get_next_in_minutes(Alarm* instance, uint32_t* minutes);

#ifdef __cplusplus
}
#endif
