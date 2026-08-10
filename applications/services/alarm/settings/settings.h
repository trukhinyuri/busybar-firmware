#pragma once

#include "interface_v1.h"

typedef AlarmSettingsV1 AlarmSettings;

/** Create the schedule's directory if it does not exist yet. */
void alarm_settings_ensure_directory(void);

bool alarm_settings_reset(AlarmSettings* settings);
bool alarm_settings_load(AlarmSettings* settings);
bool alarm_settings_save(const AlarmSettings* settings);
