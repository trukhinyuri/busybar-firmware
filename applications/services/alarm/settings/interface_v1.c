#include "interface_v1.h"

#include <furi/core/core_defines.h>

#define ALARM_DEFAULT_HOUR     (7)
#define ALARM_DEFAULT_MINUTE   (0)
#define ALARM_DEFAULT_WEEKDAYS (AlarmWeekdayWorkdays)
#define ALARM_DEFAULT_VOLUME   (80)

static bool alarm_setting_is_valid_count(const SettingProviderSetting* setting, int value) {
    UNUSED(setting);
    return (value >= 0) && (value <= (int)ALARM_MAX_COUNT);
}

static bool alarm_setting_is_valid_hour(const SettingProviderSetting* setting, int value) {
    UNUSED(setting);
    return (value >= 0) && (value <= 23);
}

static bool alarm_setting_is_valid_minute(const SettingProviderSetting* setting, int value) {
    UNUSED(setting);
    return (value >= 0) && (value <= 59);
}

static bool alarm_setting_is_valid_weekdays(const SettingProviderSetting* setting, int value) {
    UNUSED(setting);
    return (value >= 0) && (value <= (int)AlarmWeekdayEveryDay);
}

static bool alarm_setting_is_valid_volume(const SettingProviderSetting* setting, int value) {
    UNUSED(setting);
    return (value >= (int)ALARM_VOLUME_MIN) && (value <= (int)ALARM_VOLUME_MAX);
}

/**
 * Field layout shared by every persisted alarm.
 *
 * Offsets are relative to AlarmSettingsEntryV1, so the same descriptor is reused
 * for each entry and only the entry's own offset differs.
 */
static const SettingProviderSetting alarm_v1_entry_settings[] = {
    [AlarmEntrySettingV1IdxEnabled] =
        {
            .name = "enabled",
            .interface =
                &(const SettingProviderBoolInterface){
                    .default_value = false,
                },
            .field_offset = offsetof(AlarmSettingsEntryV1, enabled),
            .type = SettingProviderSettingTypeBool,
        },
    [AlarmEntrySettingV1IdxHour] =
        {
            .name = "hour",
            .interface =
                &(const SettingProviderIntInterface){
                    .is_valid_callback = alarm_setting_is_valid_hour,
                    .default_value = ALARM_DEFAULT_HOUR,
                },
            .field_offset = offsetof(AlarmSettingsEntryV1, hour),
            .type = SettingProviderSettingTypeInt,
        },
    [AlarmEntrySettingV1IdxMinute] =
        {
            .name = "minute",
            .interface =
                &(const SettingProviderIntInterface){
                    .is_valid_callback = alarm_setting_is_valid_minute,
                    .default_value = ALARM_DEFAULT_MINUTE,
                },
            .field_offset = offsetof(AlarmSettingsEntryV1, minute),
            .type = SettingProviderSettingTypeInt,
        },
    [AlarmEntrySettingV1IdxWeekdays] =
        {
            .name = "weekdays",
            .interface =
                &(const SettingProviderIntInterface){
                    .is_valid_callback = alarm_setting_is_valid_weekdays,
                    .default_value = ALARM_DEFAULT_WEEKDAYS,
                },
            .field_offset = offsetof(AlarmSettingsEntryV1, weekdays),
            .type = SettingProviderSettingTypeInt,
        },
    [AlarmEntrySettingV1IdxVolume] =
        {
            .name = "volume",
            .interface =
                &(const SettingProviderIntInterface){
                    .is_valid_callback = alarm_setting_is_valid_volume,
                    .default_value = ALARM_DEFAULT_VOLUME,
                },
            .field_offset = offsetof(AlarmSettingsEntryV1, volume),
            .type = SettingProviderSettingTypeInt,
        },
    [AlarmEntrySettingV1IdxLastHandled] =
        {
            .name = "last_handled",
            .interface =
                &(const SettingProviderIntInterface){
                    .default_value = 0,
                },
            .field_offset = offsetof(AlarmSettingsEntryV1, last_handled),
            .type = SettingProviderSettingTypeInt,
        },
};

static_assert(COUNT_OF(alarm_v1_entry_settings) == AlarmEntrySettingV1IdxsCount);

#define ALARM_ENTRY_SETTING(index)                                             \
    {                                                                          \
        .name = "alarm_" #index,                                               \
        .interface = &(const SettingProviderStructInterface){                  \
            .inner_settings = alarm_v1_entry_settings,                         \
            .inner_settings_count = COUNT_OF(alarm_v1_entry_settings),         \
        },                                                                     \
        .field_offset = offsetof(AlarmSettingsV1, entries[index]),             \
        .type = SettingProviderSettingTypeStruct,                              \
    }

const SettingProviderSetting alarm_v1_settings[] = {
    [AlarmSettingV1IdxCount] =
        {
            .name = "count",
            .interface =
                &(const SettingProviderIntInterface){
                    .is_valid_callback = alarm_setting_is_valid_count,
                    .default_value = 0,
                },
            .field_offset = offsetof(AlarmSettingsV1, count),
            .type = SettingProviderSettingTypeInt,
        },
    [AlarmSettingV1IdxEntry0] = ALARM_ENTRY_SETTING(0),
    [AlarmSettingV1IdxEntry1] = ALARM_ENTRY_SETTING(1),
    [AlarmSettingV1IdxEntry2] = ALARM_ENTRY_SETTING(2),
    [AlarmSettingV1IdxEntry3] = ALARM_ENTRY_SETTING(3),
    [AlarmSettingV1IdxEntry4] = ALARM_ENTRY_SETTING(4),
};

static_assert(COUNT_OF(alarm_v1_settings) == AlarmSettingV1IdxsCount);
static_assert(AlarmSettingV1IdxsCount == ALARM_MAX_COUNT + 1);

const SettingProviderSetting alarm_v1_settings_root = {
    .name = NULL,
    .interface =
        &(const SettingProviderStructInterface){
            .inner_settings = alarm_v1_settings,
            .inner_settings_count = COUNT_OF(alarm_v1_settings),
        },
    .field_offset = 0,
    .type = SettingProviderSettingTypeStruct,
};
