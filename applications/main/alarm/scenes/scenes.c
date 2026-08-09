#include "scenes.h"

#include <furi/core/core_defines.h>

#include <assert.h>

extern const Scene alarm_app_scene_list;
extern const Scene alarm_app_scene_edit;

const Scene* const alarm_app_scenes[] = {
    [AlarmAppSceneIdxList] = &alarm_app_scene_list,
    [AlarmAppSceneIdxEdit] = &alarm_app_scene_edit,
};

static_assert(COUNT_OF(alarm_app_scenes) == AlarmAppSceneIdxsCount);
