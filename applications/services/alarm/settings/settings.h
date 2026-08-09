#pragma once

#include "interface_v1.h"

typedef AlarmSettingsV1 AlarmSettings;

bool alarm_settings_reset(AlarmSettings* settings);
bool alarm_settings_load(AlarmSettings* settings);
bool alarm_settings_save(const AlarmSettings* settings);
