#pragma once

#include "scenes/scenes.h"

#include <alarm/alarm.h>

#include <desktop/desktop.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/modules/flex_layout.h>
#include <gui/modules/nav_bar.h>
#include <gui/modules/transition_overlay.h>
#include <time/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALARM_APP_TAG  "AlarmApp"
#define THIS_APP_NAME  "alarm"

typedef enum {
    AlarmAppEventTimerUpdate,

    AlarmAppEventSceneEventsStart,
} AlarmAppEvent;

typedef struct {
    FuriEventLoop* event_loop;
    FuriMessageQueue* input_queue;
    FuriMessageQueue* event_queue;
    FuriEventLoopTimer* timer;
    SceneManager* scene_manager;

    Gui* gui;
    Time* time;
    Alarm* alarm;
    Desktop* desktop;

    /** Index of the alarm being edited, or ALARM_MAX_COUNT for a new one. */
    size_t editing_index;

    /** Working copy of the alarm under edit. */
    AlarmEntry editing_entry;

    /* Front layout */
    TransitionOverlay* front_transition_overlay;
    Widget* front_scene_window;

    /* Back layout */
    FlexLayout* back_container;
    NavBar* back_nav_bar;
    Widget* back_scene_window;
} AlarmApp;

/**
 * @brief Post a custom event to the application scene manager.
 *
 * @param[in,out] instance pointer to the AlarmApp instance
 * @param[in] event event identifier
 */
void alarm_app_fire_event(AlarmApp* instance, uint32_t event);

#ifdef __cplusplus
}
#endif
