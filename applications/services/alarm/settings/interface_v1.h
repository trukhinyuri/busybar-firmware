#pragma once

#include "../alarm.h"

#include <setting_provider.h>

/**
 * One persisted alarm.
 *
 * `last_handled` stores the wall-clock minute key (see alarm_i.h) of the most
 * recent occurrence the service has already dealt with. Persisting it is what
 * stops a reboot from re-ringing an alarm that was already dismissed, and what
 * lets the service tell a missed alarm from a due one.
 */
typedef struct {
    bool enabled;
    int hour;
    int minute;
    int weekdays;
    int volume;
    int last_handled;
} AlarmSettingsEntryV1;

typedef enum {
    AlarmEntrySettingV1IdxEnabled,
    AlarmEntrySettingV1IdxHour,
    AlarmEntrySettingV1IdxMinute,
    AlarmEntrySettingV1IdxWeekdays,
    AlarmEntrySettingV1IdxVolume,
    AlarmEntrySettingV1IdxLastHandled,

    AlarmEntrySettingV1IdxsCount,
} AlarmEntrySettingV1Idx;

typedef struct {
    int count;
    AlarmSettingsEntryV1 entries[ALARM_MAX_COUNT];
} AlarmSettingsV1;

typedef enum {
    AlarmSettingV1IdxCount,
    AlarmSettingV1IdxEntry0,
    AlarmSettingV1IdxEntry1,
    AlarmSettingV1IdxEntry2,
    AlarmSettingV1IdxEntry3,
    AlarmSettingV1IdxEntry4,

    AlarmSettingV1IdxsCount,
} AlarmSettingV1Idx;

extern const SettingProviderSetting alarm_v1_settings[];
extern const SettingProviderSetting alarm_v1_settings_root;
