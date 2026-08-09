#pragma once

#include <furi.h>

typedef enum {
    AppsMenuEntryIdxClock,
    AppsMenuEntryIdxAlarm,
    AppsMenuEntryIdxComingSoon,

    AppsMenuEntryIdxsCount,
} AppsMenuEntryIdx;

extern const char* const apps_menu_entries[];
