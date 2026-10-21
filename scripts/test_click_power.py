#!/usr/bin/env python3
"""Compile the actual X4 Pro migration, click branch and wake-release guard with input adapters."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
source=(root/'src/main.cpp').read_text()
out=root/'build/click-power-test';out.mkdir(parents=True,exist_ok=True)
def block(start):
    begin=source.index(start);brace=source.index('{',begin);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[begin:end]
migration=block('  if (!SETTINGS.weatherClickPowerApplied)')
wake=block('  if (wakePowerReleasePending &&')
begin=source.index('  const bool x4ProReleaseSleep =')
end=source.index('  // On X4 Pro with SLEEP',begin)
click=source[begin:end]
assert '!x4ProReleaseSleep && !x4ProAwaitingClickWindow' in source
assert '!wakeHoldVerified && SETTINGS.shortPwrBtn != CrossPointSettings::SHORT_PWRBTN::SLEEP' in source
code=r'''
#include <cassert>
#include <cstdio>
unsigned long now=3000,allowSleepAt=2000;
unsigned long millis(){return now;}
bool x4=true,wakePowerReleasePending=false;
struct BoardConfig{static bool isX4Pro(){return x4;}};
struct CrossPointSettings{enum SHORT_PWRBTN{IGNORE,SLEEP,FORCE_REFRESH};};
struct Settings{int shortPwrBtn=CrossPointSettings::IGNORE,doubleClickPwrLight=1,saves=0;bool weatherClickPowerApplied=false;void saveToFile(){++saves;}} SETTINGS;
struct HalGPIO{enum{BTN_POWER,BTN_DOWN};};
struct GPIO{bool power=false,down=false;bool isPressed(int i){return i==HalGPIO::BTN_POWER?power:down;}}gpio;
struct MappedInputManager{enum class Button{Power};bool release=false;bool wasReleased(Button){return release;}}mappedInputManager;
int slept=0;
void enterDeepSleep(){++slept;}
void migrate(){MIGRATION}
void frame(bool powerReleasedSinceWake){WAKE
 const bool x4ProDoubleClickPwrLight=x4&&SETTINGS.doubleClickPwrLight;
 CLICK
}
int main(){
 migrate();assert(SETTINGS.shortPwrBtn==CrossPointSettings::SLEEP&&!SETTINGS.doubleClickPwrLight&&SETTINGS.saves==1);
 migrate();assert(SETTINGS.saves==1);
 for(int held=0;held<2;++held)for(int release=0;release<2;++release)for(int ready=0;ready<2;++ready)for(int down=0;down<2;++down)for(int guarded=0;guarded<2;++guarded){
  gpio.power=held;gpio.down=down;mappedInputManager.release=release;now=guarded?1000:3000;slept=0;
  frame(ready);assert(slept==int(release&&ready&&!down&&!guarded));
 }
 gpio.power=false;gpio.down=false;mappedInputManager.release=true;now=3000;wakePowerReleasePending=true;slept=0;
 frame(true);assert(!slept&&!wakePowerReleasePending); // wake release cannot re-sleep
 frame(true);assert(slept==1); // a later, separate click sleeps
 SETTINGS.doubleClickPwrLight=1;slept=0;frame(true);assert(!slept);
 SETTINGS.doubleClickPwrLight=0;x4=false;frame(true);assert(!slept);
 x4=true;SETTINGS.shortPwrBtn=CrossPointSettings::FORCE_REFRESH;frame(true);assert(!slept);
 migrate();assert(SETTINGS.shortPwrBtn==CrossPointSettings::FORCE_REFRESH); // later choices retained
 puts("Production power migration/click branch/wake-release guard: 32 input combinations, startup guard, screenshot, wake release, double-click and other-board preservation passed");
}
'''.replace('MIGRATION',migration).replace('WAKE',wake).replace('CLICK',click)
(out/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++20','-fsanitize=undefined',str(out/'test.cpp'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
