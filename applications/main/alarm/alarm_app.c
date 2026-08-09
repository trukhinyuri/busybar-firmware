/**
 * @file alarm_app.c
 * @brief Alarm configuration application.
 *
 * The application only presents and edits the schedule. Arming, firing and
 * dismissal all live in the alarm service, so closing this application — or
 * never opening it — has no effect on whether an alarm rings.
 */
#include "alarm_app_i.h"

#include <furi.h>
#include <storage/storage.h>

#define TAG ALARM_APP_TAG

#define INPUT_QUEUE_CAPACITY   (8U)
#define INPUT_QUEUE_TIMEOUT_MS (3000U)

#define EVENT_QUEUE_CAPACITY   (8U)
#define EVENT_QUEUE_TIMEOUT_MS (3000U)

#define TIMER_INTERVAL_MS (500U)

static void alarm_app_input_queue_callback(FuriEventLoopObject* object, void* context) {
    UNUSED(object);
    furi_assert(context);

    AlarmApp* instance = context;

    InputEvent event;
    while(furi_message_queue_get(instance->input_queue, &event, 0) == FuriStatusOk) {
        if(event.type == InputTypeShort && event.key == InputKeyBack) {
            if(!scene_manager_handle_back_event(instance->scene_manager)) {
                desktop_replace_current_app(instance->desktop, "apps_menu", THIS_APP_NAME);
            }
        }
    }
}

static void alarm_app_event_queue_callback(FuriEventLoopObject* object, void* context) {
    UNUSED(object);
    furi_assert(context);

    AlarmApp* instance = context;

    uint32_t event;
    while(furi_message_queue_get(instance->event_queue, &event, 0) == FuriStatusOk) {
        scene_manager_handle_custom_event(instance->scene_manager, event);
    }
}

static bool alarm_app_input_callback(const InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    AlarmApp* instance = context;

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        if(furi_message_queue_put(instance->input_queue, event, INPUT_QUEUE_TIMEOUT_MS) !=
           FuriStatusOk) {
            FURI_LOG_E(TAG, "Failed to put an item into input queue.");
        }

        return true;
    }

    return false;
}

static void alarm_app_timer_callback(void* context) {
    AlarmApp* instance = context;

    alarm_app_fire_event(instance, AlarmAppEventTimerUpdate);
}

static AlarmApp* alarm_app_alloc(void) {
    AlarmApp* instance = malloc(sizeof(*instance));

    instance->event_loop = furi_event_loop_alloc();
    instance->input_queue = furi_message_queue_alloc(INPUT_QUEUE_CAPACITY, sizeof(InputEvent));
    instance->event_queue = furi_message_queue_alloc(EVENT_QUEUE_CAPACITY, sizeof(uint32_t));
    instance->scene_manager =
        scene_manager_alloc(alarm_app_scenes, AlarmAppSceneIdxsCount, instance);

    instance->gui = furi_record_open(RECORD_GUI);
    instance->time = furi_record_open(RECORD_TIME);
    instance->alarm = furi_record_open(RECORD_ALARM);
    instance->desktop = furi_record_open(RECORD_DESKTOP);

    instance->editing_index = ALARM_MAX_COUNT;
    alarm_get_default_entry(&instance->editing_entry);

    instance->timer = furi_event_loop_timer_alloc(
        instance->event_loop, alarm_app_timer_callback, FuriEventLoopTimerTypePeriodic, instance);

    with_gui(instance->gui, {
        GuiLayer* layer = gui_get_layer(instance->gui, GuiLayerIdMain);
        gui_layer_add_input_callback(layer, alarm_app_input_callback, instance);

        Widget* front_root = gui_layer_get_root_widget(layer, GuiDisplayIdFront);
        instance->front_scene_window = widget_alloc(front_root);

        instance->front_transition_overlay = transition_overlay_alloc(front_root);
        transition_overlay_set_pressed_widget(
            instance->front_transition_overlay, instance->front_scene_window);

        Widget* back_root = gui_layer_get_root_widget(layer, GuiDisplayIdBack);
        instance->back_container = flex_layout_alloc(back_root, FlexLayoutTypeColumn);

        instance->back_nav_bar = nav_bar_alloc(flex_layout_get_base(instance->back_container));
        nav_bar_set_header_image(
            instance->back_nav_bar, SHARED_IMG_PATH("apps_menu_back_12x12.image"));
        nav_bar_push_location(instance->back_nav_bar, "ALARM");
        widget_set_height(nav_bar_get_base(instance->back_nav_bar), 14);
        widget_set_margin(nav_bar_get_base(instance->back_nav_bar), 1, 0, 0, 2);

        instance->back_scene_window = widget_alloc(flex_layout_get_base(instance->back_container));
        flex_layout_set_child_widget_grow(
            instance->back_container, instance->back_scene_window, 1);
    });

    furi_event_loop_subscribe_message_queue(
        instance->event_loop,
        instance->input_queue,
        FuriEventLoopEventIn,
        alarm_app_input_queue_callback,
        instance);

    furi_event_loop_subscribe_message_queue(
        instance->event_loop,
        instance->event_queue,
        FuriEventLoopEventIn,
        alarm_app_event_queue_callback,
        instance);

    furi_event_loop_timer_start(instance->timer, furi_ms_to_ticks(TIMER_INTERVAL_MS));

    scene_manager_next_scene(instance->scene_manager, AlarmAppSceneIdxList);

    return instance;
}

static void alarm_app_free(AlarmApp* instance) {
    scene_manager_free(instance->scene_manager);

    with_gui(instance->gui, {
        GuiLayer* layer = gui_get_layer(instance->gui, GuiLayerIdMain);
        gui_layer_remove_input_callback(layer, alarm_app_input_callback);

        transition_overlay_free(instance->front_transition_overlay);

        widget_free(instance->front_scene_window);
        flex_layout_free(instance->back_container);
    });

    furi_record_close(RECORD_DESKTOP);
    furi_record_close(RECORD_ALARM);
    furi_record_close(RECORD_TIME);
    furi_record_close(RECORD_GUI);

    furi_event_loop_unsubscribe(instance->event_loop, instance->input_queue);
    furi_event_loop_unsubscribe(instance->event_loop, instance->event_queue);

    furi_message_queue_free(instance->input_queue);
    furi_message_queue_free(instance->event_queue);

    furi_event_loop_timer_free(instance->timer);
    furi_event_loop_free(instance->event_loop);

    free(instance);
}

int32_t alarm_app_entry(void* argument) {
    UNUSED(argument);

    AlarmApp* instance = alarm_app_alloc();

    furi_event_loop_run(instance->event_loop);

    alarm_app_free(instance);

    return 0;
}

void alarm_app_fire_event(AlarmApp* instance, uint32_t event) {
    furi_assert(instance);

    if(furi_message_queue_put(instance->event_queue, &event, EVENT_QUEUE_TIMEOUT_MS) !=
       FuriStatusOk) {
        FURI_LOG_E(TAG, "Failed to put an item into event queue.");
    }
}
