#pragma once

#include <gui/scene_manager.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AlarmAppSceneIdxList,
    AlarmAppSceneIdxEdit,

    AlarmAppSceneIdxsCount,
} AlarmAppSceneIdx;

extern const Scene* const alarm_app_scenes[];

#ifdef __cplusplus
}
#endif
