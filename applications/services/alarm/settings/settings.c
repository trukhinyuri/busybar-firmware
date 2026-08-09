#include "settings.h"

#include <storage/storage.h>

#define ALARM_SETTINGS_FILE_PATH APP_DATA_PATH("settings.json")
#define ALARM_SETTINGS_VERSION   1
#define ALARM_SETTINGS_ROOT      alarm_v1_settings_root

bool alarm_settings_reset(AlarmSettings* settings) {
    SettingProvider* provider =
        setting_provider_alloc(ALARM_SETTINGS_FILE_PATH, ALARM_SETTINGS_VERSION, NULL, 0);
    bool is_successful = setting_provider_reset(provider, &ALARM_SETTINGS_ROOT, settings);
    setting_provider_free(provider);

    return is_successful;
}

bool alarm_settings_load(AlarmSettings* settings) {
    furi_check(settings);

    SettingProvider* provider =
        setting_provider_alloc(ALARM_SETTINGS_FILE_PATH, ALARM_SETTINGS_VERSION, NULL, 0);
    bool is_successful = setting_provider_load(provider, &ALARM_SETTINGS_ROOT, settings);
    setting_provider_free(provider);

    return is_successful;
}

bool alarm_settings_save(const AlarmSettings* settings) {
    furi_check(settings);

    SettingProvider* provider =
        setting_provider_alloc(ALARM_SETTINGS_FILE_PATH, ALARM_SETTINGS_VERSION, NULL, 0);
    bool is_successful = setting_provider_save(provider, &ALARM_SETTINGS_ROOT, settings);
    setting_provider_free(provider);

    return is_successful;
}
