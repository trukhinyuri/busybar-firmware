/**
 * @file alarm_ring.c
 * @brief Ringing presentation: display takeover, audio and the dismissal challenge.
 */
#include "alarm_i.h"

#include <furi_hal_random.h>

/**
 * Pick a fresh challenge target, guaranteed to differ from the current value so
 * that the wheel always has to be moved.
 */
static void alarm_ring_view_new_challenge(AlarmRingView* view) {
    const int span = ALARM_CHALLENGE_MAX - ALARM_CHALLENGE_MIN + 1;

    view->challenge_value = ALARM_CHALLENGE_MIN + (int)(furi_hal_random_get() % (uint32_t)span);

    do {
        view->challenge_target =
            ALARM_CHALLENGE_MIN + (int)(furi_hal_random_get() % (uint32_t)span);
    } while(view->challenge_target == view->challenge_value);
}

void alarm_ring_view_start(Alarm* instance, const AlarmEntry* entry) {
    furi_assert(instance);
    furi_assert(entry);

    AlarmRingView* view = &instance->ring_view;
    if(view->active) return;

    alarm_ring_view_new_challenge(view);

    /*
     * Bring the panels out of sleep first. low_power only sleeps the displays,
     * so the service has been running all along; this just makes it visible.
     */
    low_power_lock(instance->low_power);

    with_gui(instance->gui, {
        GuiLayer* layer = gui_get_layer(instance->gui, GuiLayerIdTop);

        Widget* front_root = gui_layer_get_root_widget(layer, GuiDisplayIdFront);
        view->front_time = label_alloc(front_root);
        label_set_text_font_size(view->front_time, LabelFontSizeLarge);
        label_set_text_align(view->front_time, TextAlignCenter);
        widget_set_size_content(label_get_base(view->front_time));
        widget_set_align(label_get_base(view->front_time), AlignCenter);

        Widget* back_root = gui_layer_get_root_widget(layer, GuiDisplayIdBack);

        view->back_title = label_alloc(back_root);
        label_set_text(view->back_title, "WAKE UP");
        label_set_text_font_size(view->back_title, LabelFontSizeLarge);
        label_set_text_align(view->back_title, TextAlignCenter);
        widget_set_size_content(label_get_base(view->back_title));
        widget_set_align(label_get_base(view->back_title), AlignTopMid);

        view->back_time = label_alloc(back_root);
        label_set_text_font_size(view->back_time, LabelFontSizeNormal);
        label_set_text_align(view->back_time, TextAlignCenter);
        widget_set_size_content(label_get_base(view->back_time));
        widget_set_align(label_get_base(view->back_time), AlignCenter);

        view->back_challenge = label_alloc(back_root);
        label_set_text_font_size(view->back_challenge, LabelFontSizeNormal);
        label_set_text_align(view->back_challenge, TextAlignCenter);
        widget_set_size_content(label_get_base(view->back_challenge));
        widget_set_align(label_get_base(view->back_challenge), AlignBottomMid);
    });

    view->active = true;
    alarm_ring_view_update(instance, entry);

    audio_enable(instance->audio);
    audio_set_volume(instance->audio, (entry->volume / 100.0f) * ALARM_RAMP_START_FACTOR);
    audio_play_file(instance->audio, ALARM_SOUND_FILE);
}

void alarm_ring_view_update(Alarm* instance, const AlarmEntry* entry) {
    furi_assert(instance);
    furi_assert(entry);

    AlarmRingView* view = &instance->ring_view;
    if(!view->active) return;

    with_gui(instance->gui, {
        label_set_text_fmt(view->front_time, "%02u:%02u", entry->hour, entry->minute);
        label_set_text_fmt(view->back_time, "%02u:%02u", entry->hour, entry->minute);
        label_set_text_fmt(
            view->back_challenge,
            "Set %d  ->  %02d",
            view->challenge_target,
            view->challenge_value);
    });
}

void alarm_ring_view_stop(Alarm* instance) {
    furi_assert(instance);

    AlarmRingView* view = &instance->ring_view;
    if(!view->active) return;

    audio_stop(instance->audio);
    audio_disable(instance->audio);

    with_gui(instance->gui, {
        label_free(view->front_time);
        label_free(view->back_title);
        label_free(view->back_time);
        label_free(view->back_challenge);
    });

    view->front_time = NULL;
    view->back_title = NULL;
    view->back_time = NULL;
    view->back_challenge = NULL;
    view->active = false;

    /* Release the displays so the device can go back to sleeping them. */
    low_power_unlock(instance->low_power);
}

/**
 * @brief Apply an input event to the challenge.
 *
 * @return true when the challenge was solved and the alarm should stop.
 */
bool alarm_ring_view_handle_input(Alarm* instance, const InputEvent* event) {
    furi_assert(instance);
    furi_assert(event);

    AlarmRingView* view = &instance->ring_view;
    if(!view->active) return false;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    const int span = ALARM_CHALLENGE_MAX - ALARM_CHALLENGE_MIN + 1;

    if(event->key == InputKeyUp) {
        view->challenge_value = ALARM_CHALLENGE_MIN +
                                (view->challenge_value - ALARM_CHALLENGE_MIN + 1) % span;
        return false;
    }

    if(event->key == InputKeyDown) {
        view->challenge_value =
            ALARM_CHALLENGE_MIN + (view->challenge_value - ALARM_CHALLENGE_MIN + span - 1) % span;
        return false;
    }

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        if(view->challenge_value == view->challenge_target) return true;

        /* A wrong answer costs a new target, so guessing is not a strategy. */
        alarm_ring_view_new_challenge(view);
        return false;
    }

    return false;
}
