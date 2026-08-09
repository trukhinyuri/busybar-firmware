/**
 * @file alarm.c
 * @brief Autonomous alarm clock service.
 *
 * Runs for the lifetime of the firmware. Because it is a service rather than an
 * application, it keeps evaluating the schedule while another application is in
 * the foreground, while the mode selector is in any position, and while the
 * displays are asleep.
 *
 * @note Locking. `mutex` guards the schedule only. Display and audio calls are
 * always made without it, because the configuration application necessarily
 * calls this service's API from inside the GUI lock. Holding the mutex across a
 * GUI call here would close that cycle and deadlock the device.
 */
#include "alarm_i.h"

#include <input/input.h>

#include <stdio.h>
#include <string.h>

#define INPUT_QUEUE_CAPACITY (8U)

/** Sentinel for "no alarm is ringing". */
#define ALARM_RINGING_NONE (ALARM_MAX_COUNT)

static void alarm_publish(Alarm* instance, AlarmEventType type) {
    const AlarmEvent event = {.type = type};
    furi_pubsub_publish(instance->pubsub, (void*)&event);
}

/** Persist the current schedule. Callers must hold the mutex. */
static void alarm_persist(Alarm* instance) {
    if(!alarm_settings_save(&instance->settings)) {
        FURI_LOG_E(ALARM_TAG, "Failed to save the alarm schedule");
    }
}

static bool alarm_read_now(Alarm* instance, int* now_key, int* weekday) {
    const LocalTime local = time_get_local_time(instance->time);

    /*
     * A year before 2001 means the RTC never received a valid time. Scheduling
     * against it would ring at an arbitrary moment, so refuse instead: an alarm
     * that visibly does not arm beats one that quietly fires at the wrong time.
     */
    if(local.dt.year < 2001) return false;

    *now_key = alarm_wall_minute_key(
        local.dt.year, local.dt.month, local.dt.dayofmonth, local.dt.hour, local.dt.minute);
    *weekday = local.dt.dayofweek;

    return true;
}

/**
 * Evaluate the schedule and report the alarm that should start ringing.
 *
 * An occurrence that is due but older than the catch-up window is recorded as
 * handled without ringing, which turns "the device was off all night" into a
 * missed alarm rather than one that goes off at breakfast.
 *
 * Callers must hold the mutex.
 *
 * @return index of the alarm to ring, or ALARM_RINGING_NONE
 */
static size_t alarm_evaluate_schedule(Alarm* instance) {
    int now_key = 0;
    int weekday = 0;

    if(!alarm_read_now(instance, &now_key, &weekday)) return ALARM_RINGING_NONE;

    size_t to_ring = ALARM_RINGING_NONE;
    bool dirty = false;

    for(size_t i = 0; i < (size_t)instance->settings.count; i++) {
        AlarmSettingsEntryV1* stored = &instance->settings.entries[i];
        if(!stored->enabled) continue;

        AlarmEntry entry;
        alarm_entry_from_settings(stored, &entry);

        int due_key = 0;
        if(!alarm_previous_due_key(&entry, now_key, weekday, &due_key)) continue;
        if(due_key <= stored->last_handled) continue;

        stored->last_handled = due_key;
        dirty = true;

        if((now_key - due_key) > ALARM_CATCHUP_MINUTES) {
            FURI_LOG_W(
                ALARM_TAG, "Alarm %u missed by %d minutes", (unsigned)i, now_key - due_key);
            continue;
        }

        if(to_ring == ALARM_RINGING_NONE) to_ring = i;
    }

    if(dirty) alarm_persist(instance);

    return to_ring;
}

/** Begin ringing. Must be called without the mutex held. */
static void alarm_begin_ring(Alarm* instance, size_t index, const AlarmEntry* entry) {
    FURI_LOG_I(
        ALARM_TAG,
        "Alarm %u ringing at %02u:%02u",
        (unsigned)index,
        (unsigned)entry->hour,
        (unsigned)entry->minute);

    instance->ringing_index = index;
    instance->ringing_entry = *entry;
    instance->ringing_started_tick = furi_get_tick();
    instance->input_grab = true;

    alarm_ring_view_start(instance, entry);
    alarm_publish(instance, AlarmEventStarted);
}

/** Stop ringing. Must be called without the mutex held. */
static void alarm_end_ring(Alarm* instance) {
    if(instance->ringing_index == ALARM_RINGING_NONE) return;

    const size_t index = instance->ringing_index;
    const bool was_one_shot = instance->ringing_entry.weekdays == AlarmWeekdayNone;

    instance->input_grab = false;
    instance->ringing_index = ALARM_RINGING_NONE;

    alarm_ring_view_stop(instance);

    /* A one-shot alarm has done its job and disarms itself. */
    if(was_one_shot) {
        furi_mutex_acquire(instance->mutex, FuriWaitForever);

        if(index < (size_t)instance->settings.count) {
            instance->settings.entries[index].enabled = false;
            alarm_persist(instance);
        }

        furi_mutex_release(instance->mutex);
    }

    alarm_publish(instance, AlarmEventStopped);
}

/** Keep the volume climbing while ringing, so a quiet start still escalates. */
static void alarm_update_ramp(Alarm* instance) {
    const uint32_t elapsed = furi_get_tick() - instance->ringing_started_tick;
    const float target = instance->ringing_entry.volume / 100.0f;

    float progress = (float)elapsed / (float)furi_ms_to_ticks(ALARM_RAMP_DURATION_MS);
    if(progress > 1.0f) progress = 1.0f;

    const float level =
        target * (ALARM_RAMP_START_FACTOR + (1.0f - ALARM_RAMP_START_FACTOR) * progress);

    audio_set_volume(instance->audio, level);
}

static void alarm_tick_callback(void* context) {
    furi_assert(context);

    Alarm* instance = context;

    if(instance->ringing_index == ALARM_RINGING_NONE) {
        furi_mutex_acquire(instance->mutex, FuriWaitForever);
        const size_t to_ring = alarm_evaluate_schedule(instance);

        AlarmEntry entry;
        if(to_ring != ALARM_RINGING_NONE) {
            alarm_entry_from_settings(&instance->settings.entries[to_ring], &entry);
        }
        furi_mutex_release(instance->mutex);

        if(to_ring != ALARM_RINGING_NONE) alarm_begin_ring(instance, to_ring, &entry);

        return;
    }

    alarm_update_ramp(instance);

    const uint32_t elapsed = furi_get_tick() - instance->ringing_started_tick;
    if(elapsed >= furi_ms_to_ticks(ALARM_MAX_RING_MS)) {
        FURI_LOG_W(ALARM_TAG, "Ringing timed out without being dismissed");
        alarm_end_ring(instance);
    }
}

/**
 * GUI-thread input hook.
 *
 * Registered on GuiLayerIdTop, which is fed before GuiLayerIdMain, so returning
 * true here keeps every key away from the foreground application while ringing.
 * The event itself is handled on the service thread.
 */
static bool alarm_input_callback(const InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    Alarm* instance = context;
    if(!instance->input_grab) return false;

    furi_message_queue_put(instance->input_queue, event, 0);

    return true;
}

static void alarm_input_queue_callback(FuriEventLoopObject* object, void* context) {
    UNUSED(object);
    furi_assert(context);

    Alarm* instance = context;

    InputEvent event;
    while(furi_message_queue_get(instance->input_queue, &event, 0) == FuriStatusOk) {
        if(instance->ringing_index == ALARM_RINGING_NONE) continue;

        if(alarm_ring_view_handle_input(instance, &event)) {
            alarm_end_ring(instance);
        } else {
            alarm_ring_view_update(instance, &instance->ringing_entry);
        }
    }
}

/** Replay the sound for as long as the alarm is ringing. */
static void alarm_audio_callback(const void* message, void* context) {
    furi_assert(message);
    furi_assert(context);

    const AudioEvent* event = message;
    Alarm* instance = context;

    if(event->type != AudioEventPlayEnd) return;
    if(!instance->input_grab) return;

    audio_play_file(instance->audio, ALARM_SOUND_FILE);
}

static Alarm* alarm_alloc(void) {
    Alarm* instance = malloc(sizeof(*instance));

    instance->event_loop = furi_event_loop_alloc();
    instance->input_queue = furi_message_queue_alloc(INPUT_QUEUE_CAPACITY, sizeof(InputEvent));
    instance->pubsub = furi_pubsub_alloc();
    instance->mutex = furi_mutex_alloc(FuriMutexTypeRecursive);

    instance->gui = furi_record_open(RECORD_GUI);
    instance->time = furi_record_open(RECORD_TIME);
    instance->audio = furi_record_open(RECORD_AUDIO);
    instance->low_power = furi_record_open(RECORD_LOW_POWER);

    instance->ringing_index = ALARM_RINGING_NONE;
    instance->ringing_started_tick = 0;
    instance->input_grab = false;
    memset(&instance->ringing_entry, 0, sizeof(instance->ringing_entry));
    memset(&instance->ring_view, 0, sizeof(instance->ring_view));

    if(!alarm_settings_load(&instance->settings)) {
        FURI_LOG_W(ALARM_TAG, "No stored schedule, starting empty");
        alarm_settings_reset(&instance->settings);
    }

    if(instance->settings.count < 0 || instance->settings.count > (int)ALARM_MAX_COUNT) {
        instance->settings.count = 0;
    }

    with_gui(instance->gui, {
        GuiLayer* layer = gui_get_layer(instance->gui, GuiLayerIdTop);
        gui_layer_add_input_callback(layer, alarm_input_callback, instance);
    });

    furi_pubsub_subscribe(audio_get_pubsub(instance->audio), alarm_audio_callback, instance);

    furi_event_loop_subscribe_message_queue(
        instance->event_loop,
        instance->input_queue,
        FuriEventLoopEventIn,
        alarm_input_queue_callback,
        instance);

    instance->tick_timer = furi_event_loop_timer_alloc(
        instance->event_loop, alarm_tick_callback, FuriEventLoopTimerTypePeriodic, instance);

    return instance;
}

int32_t alarm_srv(void* arg) {
    UNUSED(arg);

    Alarm* instance = alarm_alloc();
    furi_record_create(RECORD_ALARM, instance);

    furi_event_loop_timer_start(instance->tick_timer, furi_ms_to_ticks(ALARM_TICK_PERIOD_MS));
    furi_event_loop_run(instance->event_loop);

    return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

FuriPubSub* alarm_get_pubsub(Alarm* instance) {
    furi_check(instance);
    return instance->pubsub;
}

size_t alarm_get_count(Alarm* instance) {
    furi_check(instance);

    furi_mutex_acquire(instance->mutex, FuriWaitForever);
    const size_t count = (size_t)instance->settings.count;
    furi_mutex_release(instance->mutex);

    return count;
}

bool alarm_get_entry(Alarm* instance, size_t index, AlarmEntry* entry) {
    furi_check(instance);
    furi_check(entry);

    furi_mutex_acquire(instance->mutex, FuriWaitForever);

    const bool found = index < (size_t)instance->settings.count;
    if(found) alarm_entry_from_settings(&instance->settings.entries[index], entry);

    furi_mutex_release(instance->mutex);

    return found;
}

/**
 * Re-arm an alarm after an edit. Callers must hold the mutex.
 *
 * The catch-up bookkeeping is moved to the most recent past occurrence so an
 * edited alarm neither inherits a stale marker nor immediately fires for a time
 * that has just passed.
 */
static void alarm_rearm(Alarm* instance, size_t index) {
    int now_key = 0;
    int weekday = 0;

    if(!alarm_read_now(instance, &now_key, &weekday)) return;

    AlarmEntry entry;
    alarm_entry_from_settings(&instance->settings.entries[index], &entry);

    int due_key = 0;
    instance->settings.entries[index].last_handled =
        alarm_previous_due_key(&entry, now_key, weekday, &due_key) ? due_key : now_key;
}

bool alarm_set_entry(Alarm* instance, size_t index, const AlarmEntry* entry) {
    furi_check(instance);
    furi_check(entry);

    furi_mutex_acquire(instance->mutex, FuriWaitForever);

    const bool updated = index < (size_t)instance->settings.count;
    if(updated) {
        alarm_entry_to_settings(entry, &instance->settings.entries[index]);
        alarm_rearm(instance, index);
        alarm_persist(instance);
    }

    furi_mutex_release(instance->mutex);

    if(updated) alarm_publish(instance, AlarmEventScheduleChanged);

    return updated;
}

bool alarm_add_entry(Alarm* instance, const AlarmEntry* entry) {
    furi_check(instance);
    furi_check(entry);

    furi_mutex_acquire(instance->mutex, FuriWaitForever);

    const bool added = instance->settings.count < (int)ALARM_MAX_COUNT;
    if(added) {
        const size_t index = (size_t)instance->settings.count;

        memset(&instance->settings.entries[index], 0, sizeof(AlarmSettingsEntryV1));
        alarm_entry_to_settings(entry, &instance->settings.entries[index]);
        instance->settings.count++;

        alarm_rearm(instance, index);
        alarm_persist(instance);
    }

    furi_mutex_release(instance->mutex);

    if(added) alarm_publish(instance, AlarmEventScheduleChanged);

    return added;
}

bool alarm_remove_entry(Alarm* instance, size_t index) {
    furi_check(instance);

    /* Stop first: a ringing alarm must not outlive its own definition. */
    if(instance->ringing_index == index) alarm_end_ring(instance);

    furi_mutex_acquire(instance->mutex, FuriWaitForever);

    const bool removed = index < (size_t)instance->settings.count;
    if(removed) {
        for(size_t i = index; i + 1 < (size_t)instance->settings.count; i++) {
            instance->settings.entries[i] = instance->settings.entries[i + 1];
        }

        instance->settings.count--;
        memset(
            &instance->settings.entries[instance->settings.count],
            0,
            sizeof(AlarmSettingsEntryV1));

        alarm_persist(instance);
    }

    furi_mutex_release(instance->mutex);

    if(removed) alarm_publish(instance, AlarmEventScheduleChanged);

    return removed;
}

void alarm_get_default_entry(AlarmEntry* entry) {
    furi_check(entry);

    entry->enabled = true;
    entry->hour = 7;
    entry->minute = 0;
    entry->weekdays = AlarmWeekdayWorkdays;
    entry->volume = 80;
}

bool alarm_is_ringing(Alarm* instance) {
    furi_check(instance);
    return instance->ringing_index != ALARM_RINGING_NONE;
}

void alarm_format_weekdays(uint8_t weekdays, char* buffer, size_t buffer_size) {
    furi_check(buffer);
    furi_check(buffer_size > 0);

    static const char* const names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

    if(weekdays == AlarmWeekdayNone) {
        snprintf(buffer, buffer_size, "Once");
        return;
    }
    if(weekdays == AlarmWeekdayEveryDay) {
        snprintf(buffer, buffer_size, "Every day");
        return;
    }
    if(weekdays == AlarmWeekdayWorkdays) {
        snprintf(buffer, buffer_size, "Workdays");
        return;
    }
    if(weekdays == AlarmWeekdayWeekend) {
        snprintf(buffer, buffer_size, "Weekend");
        return;
    }

    buffer[0] = '\0';
    size_t used = 0;

    for(size_t i = 0; i < COUNT_OF(names); i++) {
        if(!(weekdays & (1U << i))) continue;

        const int written =
            snprintf(buffer + used, buffer_size - used, used ? " %s" : "%s", names[i]);
        if(written <= 0 || (size_t)written >= buffer_size - used) break;

        used += (size_t)written;
    }
}

bool alarm_get_next_in_minutes(Alarm* instance, uint32_t* minutes) {
    furi_check(instance);
    furi_check(minutes);

    furi_mutex_acquire(instance->mutex, FuriWaitForever);

    int now_key = 0;
    int weekday = 0;
    bool found = false;
    int best = 0;

    if(alarm_read_now(instance, &now_key, &weekday)) {
        for(size_t i = 0; i < (size_t)instance->settings.count; i++) {
            if(!instance->settings.entries[i].enabled) continue;

            AlarmEntry entry;
            alarm_entry_from_settings(&instance->settings.entries[i], &entry);

            int due_key = 0;
            if(!alarm_next_due_key(&entry, now_key, weekday, &due_key)) continue;

            if(!found || due_key < best) {
                best = due_key;
                found = true;
            }
        }
    }

    furi_mutex_release(instance->mutex);

    if(found) *minutes = (uint32_t)(best - now_key);

    return found;
}
