/**
 * @file scene_list.c
 * @brief Alarm list: every stored alarm plus an entry for creating a new one.
 *
 * Disabled alarms are listed alongside enabled ones so that every alarm remains
 * reachable from the device, which is the whole point of running on-device.
 */
#include "../alarm_app_i.h"

#include <gui/modules/label.h>
#include <gui/modules/menu.h>
#include <storage/storage.h>

#define LIST_LABEL_SIZE (32U)

typedef struct {
    Menu* back_menu;
    Label* front_label;

    size_t shown_count;
    uint32_t selected;
} AlarmAppSceneList;

static void alarm_app_scene_list_menu_callback(uint32_t index, void* context) {
    furi_assert(context);

    AlarmApp* instance = context;
    alarm_app_fire_event(instance, AlarmAppEventSceneEventsStart + index);
}

/** Render the selected alarm's time on the front display. */
static void alarm_app_scene_list_update_front(AlarmApp* instance, AlarmAppSceneList* scene) {
    AlarmEntry entry;

    if(scene->selected < scene->shown_count &&
       alarm_get_entry(instance->alarm, scene->selected, &entry)) {
        label_set_text_fmt(scene->front_label, "%02u:%02u", entry.hour, entry.minute);
    } else {
        label_set_text(scene->front_label, "New alarm");
    }
}

static void alarm_app_scene_list_on_enter(void* context) {
    furi_assert(context);

    AlarmApp* instance = context;
    AlarmAppSceneList* scene =
        scene_manager_get_scene_data(instance->scene_manager, AlarmAppSceneIdxList);

    scene->shown_count = alarm_get_count(instance->alarm);
    if(scene->selected > scene->shown_count) scene->selected = scene->shown_count;

    with_gui(instance->gui, {
        scene->front_label = label_alloc(instance->front_scene_window);
        label_set_text_font_size(scene->front_label, LabelFontSizeLarge);
        widget_set_size_content(label_get_base(scene->front_label));
        widget_set_align(label_get_base(scene->front_label), AlignCenter);

        scene->back_menu = menu_alloc(instance->back_scene_window);

        for(size_t i = 0; i < scene->shown_count; i++) {
            AlarmEntry entry;
            if(!alarm_get_entry(instance->alarm, i, &entry)) continue;

            char label[LIST_LABEL_SIZE];
            char repeat[LIST_LABEL_SIZE];
            alarm_format_weekdays(entry.weekdays, repeat, sizeof(repeat));
            snprintf(
                label,
                sizeof(label),
                "%02u:%02u %s",
                entry.hour,
                entry.minute,
                entry.enabled ? "on" : "off");

            menu_add_item(
                scene->back_menu,
                label,
                repeat,
                SHARED_IMG_PATH("start_11x11.image"),
                i,
                alarm_app_scene_list_menu_callback,
                instance);
        }

        if(scene->shown_count < ALARM_MAX_COUNT) {
            menu_add_item(
                scene->back_menu,
                "New alarm",
                NULL,
                SHARED_IMG_PATH("setup_11x11.image"),
                scene->shown_count,
                alarm_app_scene_list_menu_callback,
                instance);
        }

        menu_set_selected_item_index(scene->back_menu, scene->selected);
        widget_set_scrollbar_enabled(menu_get_base(scene->back_menu), true);
        widget_set_visible(nav_bar_get_base(instance->back_nav_bar), true);

        alarm_app_scene_list_update_front(instance, scene);
    });
}

static void alarm_app_scene_list_on_exit(void* context) {
    furi_assert(context);

    AlarmApp* instance = context;
    AlarmAppSceneList* scene =
        scene_manager_get_scene_data(instance->scene_manager, AlarmAppSceneIdxList);

    with_gui(instance->gui, {
        menu_free(scene->back_menu);
        label_free(scene->front_label);
    });
}

static bool alarm_app_scene_list_on_event(const SceneManagerEvent* event, void* context) {
    furi_assert(context);

    AlarmApp* instance = context;
    AlarmAppSceneList* scene =
        scene_manager_get_scene_data(instance->scene_manager, AlarmAppSceneIdxList);

    if(event->type != SceneManagerEventTypeCustom) return false;

    if(event->event == AlarmAppEventTimerUpdate) {
        with_gui(instance->gui, {
            const uint32_t selected = menu_get_selected_item_index(scene->back_menu);
            if(selected != scene->selected) {
                scene->selected = selected;
                alarm_app_scene_list_update_front(instance, scene);
            }
        });

        return true;
    }

    if(event->event >= AlarmAppEventSceneEventsStart) {
        const size_t index = event->event - AlarmAppEventSceneEventsStart;

        if(index < scene->shown_count) {
            instance->editing_index = index;
            if(!alarm_get_entry(instance->alarm, index, &instance->editing_entry)) return true;
        } else {
            /* The trailing row creates a new alarm from the defaults. */
            instance->editing_index = ALARM_MAX_COUNT;
            alarm_get_default_entry(&instance->editing_entry);
        }

        with_gui(instance->gui, { nav_bar_push_location(instance->back_nav_bar, "EDIT"); });
        scene_manager_next_scene(instance->scene_manager, AlarmAppSceneIdxEdit);

        return true;
    }

    return false;
}

const Scene alarm_app_scene_list = {
    .enter_callback = alarm_app_scene_list_on_enter,
    .exit_callback = alarm_app_scene_list_on_exit,
    .event_callback = alarm_app_scene_list_on_event,
    .data_size = sizeof(AlarmAppSceneList),
};
