#!/usr/bin/env python3
"""Compile production input, coordinate transform and ActivityManager routes with HAL/task adapters.
No reimplementation of the methods under test: extracted definitions are byte-for-byte source.
"""
from pathlib import Path
import argparse, subprocess, tempfile, re
ROOT=Path(__file__).resolve().parents[1]
def method(path,signature):
    text=(ROOT/path).read_text(); start=text.index(signature); brace=text.index('{',start); depth=1; end=brace+1
    # Definitions under test contain no unmatched braces inside strings/comments.
    while depth:
        if text[end]=='{': depth+=1
        elif text[end]=='}': depth-=1
        end+=1
    return text[start:end]+'\n'
def run(output):
    output.mkdir(parents=True,exist_ok=True)
    mapped=(ROOT/'src/MappedInputManager.cpp').read_text()
    mapped=re.sub(r'^#include .*\n','',mapped,flags=re.M)
    code='''#include "MappedInputManager.h"
#include "Settings.h"
#include <BoardConfig.h>
#include <FreeInkUICore.h>
#include <GfxRenderer.h>
#include <HalFrontlight.h>
#include <algorithm>
#include <cstdlib>
#include <Performance.h>
namespace performance {bool saveSnapshot(){return true;}}
'''+mapped+'\n'+method('lib/GfxRenderer/GfxRenderer.cpp','void GfxRenderer::tapToLogical(')
    themes=[]
    for name,file in [('Base','src/components/themes/BaseTheme.h'),('Lyra','src/components/themes/lyra/LyraTheme.h'),('Lyra3','src/components/themes/lyra/LyraTheme.h'),('RoundedRaff','src/components/themes/roundedraff/RoundedRaffTheme.h')]:
        data=(ROOT/file).read_text()
        values=[int(re.search(r'\.'+field+r'\s*=\s*(\d+)',data)[1]) for field in ['topPadding','batteryBarHeight','headerHeight']]
        themes.append('{"'+name+'",'+','.join(map(str,values))+'}')
    code+='struct ThemeData {const char* name; int top,strip,height;};\nconstexpr ThemeData TEST_THEMES[]={'+','.join(themes)+'};\n'
    tests=(ROOT/'test/back_navigation/NavigationTest.cpp').read_text()
    at=tests.index('int main(')
    tests=tests[:at]+(ROOT/'test/back_navigation/RoutesTest.h').read_text()+'\n'+tests[at:]
    tests=tests.replace(' // Publication is an atomic whole rectangle', ' checkRoutes(r,g,input);\n // Publication is an atomic whole rectangle')
    geometry_source=(ROOT/'src/components/themes/BaseTheme.cpp').read_text()
    start=geometry_source.index('  constexpr int16_t headerButtonSize = 48;')
    end=geometry_source.index('  status.clockCentered =',start)
    geometry_code=geometry_source[start:end]
    geometry_helper='\nstruct HeaderMetrics {int headerHeight,batteryBarHeight;};\nstruct FontMetricsAdapter {int drop;int getLineHeight(int)const{return 32+drop*2;}int getTextHeight(int)const{return 32;}};\nstruct ScaleAdapter {int titleFontId=0;};\ninline ScaleAdapter uiScaleSpec(){return {};}\nvoid configureHeader(const FontMetricsAdapter& renderer,const HeaderMetrics& metrics,fui::HeaderProps& props){\n auto& status=props.status;\n'+geometry_code+'}\n'
    tests=tests.replace('void checkRoutes(', geometry_helper+'\nvoid checkRoutes(')
    code+=tests
    snapshot=method('src/components/UiAppHelpers.h','inline freeink::ui::InputSnapshot touchSnapshotFrom(')
    code=code.replace('#include "components/UiAppHost.h"',snapshot+'\n#include "components/UiAppHost.h"')
    host=(ROOT/'src/components/UiAppHost.cpp').read_text()
    code+=re.sub(r'^#include .*\n','',host,flags=re.M)
    code+=method('src/components/OptionPopup.h','  bool handleInput(').replace('bool handleInput(', 'bool OptionPopup::handleInput(')
    keyboard=(ROOT/'src/activities/util/KeyboardEntryActivity.cpp').read_text()
    a=keyboard.index('void KeyboardEntryActivity::loop() {');b=keyboard.index('  int tx = 0;',a)
    code+=keyboard[a:b]+'  ++fallback;\n}\n'
    wifi=(ROOT/'src/activities/network/WifiSelectionActivity.cpp').read_text()
    a=wifi.index('  if (state == WifiSelectionState::CONNECTING ||');b=wifi.index('  // Reached once',a)
    code+='void WifiSelectionActivity::loop() {\n'+wifi[a:b]+'}\n'
    code+=method('src/activities/settings/ClearCacheActivity.cpp','void ClearCacheActivity::loop()')
    for path,signature in [('src/activities/library/LibraryListActivity.cpp','void LibraryListActivity::handleBackAction('),
                           ('src/activities/settings/SettingsActivity.cpp','bool SettingsActivity::handleButtons('),
                           ('src/activities/network/UsbDriveActivity.cpp','void UsbDriveActivity::loop('),
                           ('src/activities/settings/SdFirmwareUpdateActivity.cpp','void SdFirmwareUpdateActivity::loop(')]:
        code+=method(path,signature)
    for name in ['loop()','exitActivity(', 'replaceActivity(', 'pushActivity(', 'popActivity(']:
        code+=method('src/activities/ActivityManager.cpp','void ActivityManager::'+name)
    code+=(ROOT/'src/activities/Activity.cpp').read_text().replace('#include "Activity.h"','#include "activities/Activity.h"').replace('#include "ActivityManager.h"','#include "activities/ActivityManager.h"')
    # The complete production UiAppHost gating/routing methods operate on the real SDK app.
    # Separate test uses lightweight target adapters to avoid embedded display dependencies.
    generated=output/'navigation.generated.cpp';generated.write_text(code)
    exe=output/'navigation-test'
    subprocess.run(['c++','-std=c++20','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-DNDEBUG','-DFREEINK_CAP_TOUCH=1',
       '-I'+str(ROOT/'test/back_navigation/stubs'),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'lib/Performance'),'-I'+str(ROOT/'freeink-sdk/libs/ui/FreeInkUI/include'),
       str(generated),str(ROOT/'freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(output/'geometry.csv')],check=True)
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path);args=ap.parse_args()
    if args.output:run(args.output)
    else:
        with tempfile.TemporaryDirectory() as temp:run(Path(temp))
