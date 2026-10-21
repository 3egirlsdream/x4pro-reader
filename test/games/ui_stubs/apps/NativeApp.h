#pragma once
#include "activities/Activity.h"
inline unsigned openedApps = 0;
inline void openApps(GfxRenderer&, MappedInputManager&) { ++openedApps; }
