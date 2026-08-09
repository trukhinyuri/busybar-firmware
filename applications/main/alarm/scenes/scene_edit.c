/**
 * @file scene_edit.c
 * @brief Alarm editor.
 *
 * Changes are applied to the service as they are made rather than behind a Save
 * step, so an alarm can never be left in a state the user believes was stored
 * but was not. Leaving with BACK returns to the list.
 *
 * Weekdays are seven independent switches rather than a preset selector: with a
 * wheel and three buttons, toggling a day is one action, while cycling presets
 * to reach an arbitrary combination is not possible at all.
 */
#include "../alarm_app_i.h"

#include <gui/modules/label.h>
#include <gui/modules/var_item_list.h>

#define WEEKDAY_COUNT (7U)

typedef struct {
    VarItemList* back_list;
    Label* front_label;

    VarItem* hour_item;
    VarItem* minute_item;
    VarItem* volume_item;
    VarItem* enabled_item;
    VarItem* weekday_items[WEEKDAY_COUNT];
    VarItem* delete_item;

    bool deleted;
} AlarmAppSceneEdit;

static const char* const alarm_app_weekday_names[WEEKDAY_COUNT] =
    {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

/* Not const-qualified: var_item_list_add_selector takes a non-const array. */
static const char* alarm_app_delete_choices[] = {"No", "Yes"};

static void alarm_app_scene_edit_refresh_front(AlarmApp* instance, AlarmAppSceneEdit* scene) {
    char repeat[32];
    alarm_format_weekdays(instance->editing_entry.weekdays, repeat, sizeof(repeat));

    label_set_text_fmt(
        scene->front_label,
        "%02u:%02u %s",
        instance->editing_entry.hour,
        instance->editing_entry.minute,
        repeat);
}

/**
 * Push the working copy to the service.
 *
 * A new alarm is appended on its first change and afterwards updated in place,
 * so the editor works identically whether the alarm already existed or not.
 */
static void alarm_app_scene_edit_commit(AlarmApp* instance, AlarmAppSceneEdit* scene) {
    if(instance->editing_index < ALARM_MAX_COUNT) {
        alarm_set_entry(instance->alarm, instance->editing_index, &instance->editing_entry);
    } else if(alarm_add_entry(instance->alarm, &instance->editing_entry)) {
        instance->editing_index = alarm_get_count(instance->alarm) - 1;
    } else {
        FURI_LOG_W(ALARM_APP_TAG, "Alarm storage is full");
    }

    alarm_app_scene_edit_refresh_front(instance, scene);
}

static AlarmAppSceneEdit* alarm_app_scene_edit_data(AlarmApp* instance) {
    return scene_manager_get_scene_data(instance->scene_manager, AlarmAppSceneIdxEdit);
}

static void alarm_app_scene_edit_hour_callback(VarItem* item, void* context) {
    AlarmApp* instance = context;
    AlarmAppSceneEdit* scene = alarm_app_scene_edit_data(instance);

    instance->editing_entry.hour = (uint8_t)var_item_get_value(item);
    alarm_app_scene_edit_commit(instance, scene);
}

static void alarm_app_scene_edit_minute_callback(VarItem* item, void* context) {
    AlarmApp* instance = context;
    AlarmAppSceneEdit* scene = alarm_app_scene_edit_data(instance);

    instance->editing_entry.minute = (uint8_t)var_item_get_value(item);
    alarm_app_scene_edit_commit(instance, scene);
}

static void alarm_app_scene_edit_volume_callback(VarItem* item, void* context) {
    AlarmApp* instance = context;
    AlarmAppSceneEdit* scene = alarm_app_scene_edit_data(instance);

    instance->editing_entry.volume = (uint8_t)var_item_get_value(item);
    alarm_app_scene_edit_commit(instance, scene);
}

static void alarm_app_scene_edit_enabled_callback(VarItem* item, void* context) {
    AlarmApp* instance = context;
    AlarmAppSceneEdit* scene = alarm_app_scene_edit_data(instance);

    instance->editing_entry.enabled = var_item_get_value(item) != 0;
    alarm_app_scene_edit_commit(instance, scene);
}

static void alarm_app_scene_edit_weekday_callback(VarItem* item, void* context) {
    AlarmApp* instance = context;
    AlarmAppSceneEdit* scene = alarm_app_scene_edit_data(instance);

    for(size_t i = 0; i < WEEKDAY_COUNT; i++) {
        if(scene->weekday_items[i] != item) continue;

        if(var_item_get_value(item)) {
            instance->editing_entry.weekdays |= (uint8_t)(1U << i);
        } else {
            instance->editing_entry.weekdays &= (uint8_t) ~(1U << i);
        }

        break;
    }

    alarm_app_scene_edit_commit(instance, scene);
}

static void alarm_app_scene_edit_delete_callback(VarItem* item, void* context) {
    AlarmApp* instance = context;
    AlarmAppSceneEdit* scene = alarm_app_scene_edit_data(instance);

    if(!var_item_get_value(item)) return;
    if(instance->editing_index >= ALARM_MAX_COUNT) return;

    if(alarm_remove_entry(instance->alarm, instance->editing_index)) {
        scene->deleted = true;
        alarm_app_fire_event(instance, AlarmAppEventSceneEventsStart);
    }
}

static void alarm_app_scene_edit_on_enter(void* context) {
    furi_assert(context);

    AlarmApp* instance = context;
    AlarmAppSceneEdit* scene = alarm_app_scene_edit_data(instance);

    scene->deleted = false;

    with_gui(instance->gui, {
        scene->front_label = label_alloc(instance->front_scene_window);
        label_set_text_font_size(scene->front_label, LabelFontSizeNormal);
        widget_set_size_content(label_get_base(scene->front_label));
        widget_set_align(label_get_base(scene->front_label), AlignCenter);

        scene->back_list = var_item_list_alloc(instance->back_scene_window);

        scene->hour_item = var_item_list_add_spinbox(
            scene->back_list, "Hour", NULL, 0, 23, 1, alarm_app_scene_edit_hour_callback, instance);
        var_item_set_value(scene->hour_item, instance->editing_entry.hour);

        scene->minute_item = var_item_list_add_spinbox(
            scene->back_list,
            "Minute",
            NULL,
            0,
            59,
            1,
            alarm_app_scene_edit_minute_callback,
            instance);
        var_item_set_value(scene->minute_item, instance->editing_entry.minute);

        scene->enabled_item = var_item_list_add_switch(
            scene->back_list, "Enabled", alarm_app_scene_edit_enabled_callback, instance);
        var_item_set_value(scene->enabled_item, instance->editing_entry.enabled ? 1 : 0);

        for(size_t i = 0; i < WEEKDAY_COUNT; i++) {
            scene->weekday_items[i] = var_item_list_add_switch(
                scene->back_list,
                alarm_app_weekday_names[i],
                alarm_app_scene_edit_weekday_callback,
                instance);
            var_item_set_value(
                scene->weekday_items[i],
                (instance->editing_entry.weekdays & (1U << i)) ? 1 : 0);
        }

        scene->volume_item = var_item_list_add_spinbox(
            scene->back_list,
            "Volume",
            "%",
            ALARM_VOLUME_MIN,
            ALARM_VOLUME_MAX,
            10,
            alarm_app_scene_edit_volume_callback,
            instance);
        var_item_set_value(scene->volume_item, instance->editing_entry.volume);

        scene->delete_item = var_item_list_add_selector(
            scene->back_list,
            "Delete",
            NULL,
            alarm_app_delete_choices,
            COUNT_OF(alarm_app_delete_choices),
            alarm_app_scene_edit_delete_callback,
            instance);
        var_item_set_value(scene->delete_item, 0);

        widget_set_scrollbar_enabled(var_item_list_get_base(scene->back_list), true);

        alarm_app_scene_edit_refresh_front(instance, scene);
    });

    /*
     * A brand new alarm is stored right away. Without this, an alarm the user
     * enabled but did not otherwise touch would silently never be created.
     */
    if(instance->editing_index >= ALARM_MAX_COUNT) {
        alarm_app_scene_edit_commit(instance, scene);
    }
}

static void alarm_app_scene_edit_on_exit(void* context) {
    furi_assert(context);

    AlarmApp* instance = context;
    AlarmAppSceneEdit* scene = alarm_app_scene_edit_data(instance);

    with_gui(instance->gui, {
        var_item_list_free(scene->back_list);
        label_free(scene->front_label);
    });
}

static bool alarm_app_scene_edit_on_event(const SceneManagerEvent* event, void* context) {
    furi_assert(context);

    AlarmApp* instance = context;

    if(event->type != SceneManagerEventTypeCustom) return false;

    /* Deleting the alarm removes what this scene was editing, so leave it. */
    if(event->event == AlarmAppEventSceneEventsStart) {
        scene_manager_previous_scene(instance->scene_manager);
        return true;
    }

    return false;
}

const Scene alarm_app_scene_edit = {
    .enter_callback = alarm_app_scene_edit_on_enter,
    .exit_callback = alarm_app_scene_edit_on_exit,
    .event_callback = alarm_app_scene_edit_on_event,
    .data_size = sizeof(AlarmAppSceneEdit),
};
