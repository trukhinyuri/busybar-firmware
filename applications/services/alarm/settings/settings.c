#include "settings.h"

#include <storage/storage.h>

/**
 * The schedule lives at a fixed absolute path rather than behind APP_DATA_PATH.
 *
 * The `/data` alias is expanded by the storage service using the *calling
 * thread's* application id. The alarm API is called both by this service and,
 * while alarms are being edited, by the configuration application, so the alias
 * resolved to two different directories and the schedule was silently split in
 * two: the service kept reading its own copy and every alarm created from the
 * UI disappeared on the next boot. An absolute path is the same file no matter
 * which thread asks for it.
 */
#define ALARM_SETTINGS_DIR       EXT_PATH("apps_data/alarm_service")
#define ALARM_SETTINGS_FILE_PATH ALARM_SETTINGS_DIR "/settings.json"
#define ALARM_SETTINGS_VERSION   1
#define ALARM_SETTINGS_ROOT      alarm_v1_settings_root

void alarm_settings_ensure_directory(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(storage_common_stat(storage, ALARM_SETTINGS_DIR, NULL) != FSE_OK) {
        storage_common_mkdir(storage, EXT_PATH("apps_data"));
        storage_common_mkdir(storage, ALARM_SETTINGS_DIR);
    }

    furi_record_close(RECORD_STORAGE);
}

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
