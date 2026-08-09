#pragma once

#include "alarm.h"
#include "settings/settings.h"

#include <furi.h>

#include <audio/audio.h>
#include <gui/gui.h>
#include <gui/modules/label.h>
#include <low_power/low_power.h>
#include <time/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALARM_TAG "Alarm"

/** Scheduler evaluation period. One second is enough to hit the target minute. */
#define ALARM_TICK_PERIOD_MS (1000U)

/**
 * How late an alarm may be and still ring.
 *
 * An occurrence that became due while the device was rebooting is worth ringing;
 * one from hours ago is not, and is recorded as missed instead.
 */
#define ALARM_CATCHUP_MINUTES (15)

/** Ringing stops by itself after this long so a forgotten alarm cannot flatten the battery. */
#define ALARM_MAX_RING_MS (30U * 60U * 1000U)

/** Volume ramp start, as a fraction of the alarm's configured volume. */
#define ALARM_RAMP_START_FACTOR (0.25f)

/** Time taken for the volume to reach the configured level. */
#define ALARM_RAMP_DURATION_MS (60U * 1000U)

/** Dismissal challenge bounds. */
#define ALARM_CHALLENGE_MIN (10)
#define ALARM_CHALLENGE_MAX (99)

#define ALARM_ASSETS_PATH(path) EXT_PATH("apps_assets/alarm") "/" path

/** Sound played while ringing. */
#define ALARM_SOUND_FILE ALARM_ASSETS_PATH("sounds/wake.snd")

/** Ringing user interface, owned by the service and drawn on the top GUI layer. */
typedef struct {
    bool active;

    Label* front_time;
    Label* back_title;
    Label* back_time;
    Label* back_challenge;

    int challenge_target;
    int challenge_value;
} AlarmRingView;

struct Alarm {
    FuriEventLoop* event_loop;
    FuriEventLoopTimer* tick_timer;
    FuriMessageQueue* input_queue;
    FuriPubSub* pubsub;
    FuriMutex* mutex;

    Gui* gui;
    Time* time;
    Audio* audio;
    LowPower* low_power;

    AlarmSettings settings;

    /**
     * Set while ringing so the GUI thread's input callback knows to swallow every
     * key before the foreground application can see it.
     */
    _Atomic bool input_grab;

    /** Index of the alarm currently ringing, or ALARM_MAX_COUNT when idle. */
    size_t ringing_index;

    /**
     * Snapshot of the ringing alarm.
     *
     * Ringing reads this copy rather than the shared schedule, which is what
     * lets every display and audio call run without holding the mutex. The lock
     * order is therefore always "GUI lock, then mutex", never the reverse.
     */
    AlarmEntry ringing_entry;

    uint32_t ringing_started_tick;

    AlarmRingView ring_view;
};

/**
 * @brief Convert a local date and time to a monotonic wall-clock minute key.
 *
 * The key counts minutes from the epoch in local wall time, which makes
 * comparisons and "has this already been handled" checks trivial without any
 * timezone arithmetic. Because it follows wall time rather than UTC, a DST
 * change moves alarms the way a user expects.
 *
 * @param[in] year full year
 * @param[in] month month, 1-12
 * @param[in] day day of month, 1-31
 * @param[in] hour hour, 0-23
 * @param[in] minute minute, 0-59
 * @return wall-clock minute key
 */
int alarm_wall_minute_key(int year, int month, int day, int hour, int minute);

/**
 * @brief Find the most recent occurrence of an alarm at or before a given moment.
 *
 * @param[in] entry alarm to evaluate
 * @param[in] now_key current wall-clock minute key
 * @param[in] now_weekday current weekday, Monday = 1 to Sunday = 7
 * @param[out] due_key destination for the occurrence key
 * @return true if an occurrence exists within the last week and @p due_key was filled
 */
bool alarm_previous_due_key(const AlarmEntry* entry, int now_key, int now_weekday, int* due_key);

/**
 * @brief Find the next occurrence of an alarm strictly after a given moment.
 *
 * @param[in] entry alarm to evaluate
 * @param[in] now_key current wall-clock minute key
 * @param[in] now_weekday current weekday, Monday = 1 to Sunday = 7
 * @param[out] due_key destination for the occurrence key
 * @return true if an occurrence exists within the next week and @p due_key was filled
 */
bool alarm_next_due_key(const AlarmEntry* entry, int now_key, int now_weekday, int* due_key);

/** @brief Copy a persisted alarm into the public representation. */
void alarm_entry_from_settings(const AlarmSettingsEntryV1* source, AlarmEntry* destination);

/** @brief Copy a public alarm into its persisted representation, keeping bookkeeping fields. */
void alarm_entry_to_settings(const AlarmEntry* source, AlarmSettingsEntryV1* destination);

/** @brief Build the ringing views on the top GUI layer and start audio. */
void alarm_ring_view_start(Alarm* instance, const AlarmEntry* entry);

/** @brief Update the ringing views after a challenge change. */
void alarm_ring_view_update(Alarm* instance, const AlarmEntry* entry);

/** @brief Tear the ringing views down and stop audio. */
void alarm_ring_view_stop(Alarm* instance);

/**
 * @brief Apply an input event to the dismissal challenge.
 *
 * @param[in,out] instance pointer to the Alarm service instance
 * @param[in] event input event to apply
 * @return true when the challenge was solved and ringing should stop
 */
bool alarm_ring_view_handle_input(Alarm* instance, const InputEvent* event);

#ifdef __cplusplus
}
#endif
