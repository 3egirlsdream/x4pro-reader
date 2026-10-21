#pragma once
#include <cstdint>
class GfxRenderer;
class MappedInputManager;
namespace weather {
extern bool sleepActive;
bool startSleep(GfxRenderer& renderer, MappedInputManager& input);
void finishSleep(uint32_t seconds);
}  // namespace weather
