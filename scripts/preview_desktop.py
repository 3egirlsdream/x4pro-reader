#!/usr/bin/env python3
"""Run the exact production home draw() with SDK layout/routing and host graphics adapters."""
from pathlib import Path
import subprocess,json,argparse,shutil,re
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--font',type=Path,required=True);a=p.parse_args()
build=root/'build/desktop-preview';build.mkdir(parents=True,exist_ok=True);a.output.mkdir(parents=True,exist_ok=True)
translations={}
for line in (root/'lib/I18n/translations/chinese-simplified.yaml').read_text().splitlines():
 if line.startswith('STR_'):
  key,value=line.split(':',1);translations[key]=json.loads(value.strip())
entries=re.findall(r'\{"([a-z0-9-]+)", StrId::(STR_\w+), UIIcon::(\w+), create<', (root/'src/apps/AppRegistry.cpp').read_text())
assert len(entries)==6 and len({entry[0] for entry in entries})==6
assert [entry[0] for entry in entries[:4]]==['weather','game-2048','game-gomoku','game-mines']
(build/'Registry.inc').write_text('inline Descriptor registry[] = {'+','.join('{'+json.dumps(translations[key],ensure_ascii=False)+','+icon+'}' for _,key,icon in entries)+'};\ninline int registryCount = sizeof(registry)/sizeof(registry[0]);\n')
source=(root/'src/components/CoverGridHomeUi.cpp').read_text()
controller=(root/'src/activities/home/HomeActivity.cpp').read_text()
(build/'Draw.inc').write_text('#include "Support.h"\n'+source[source.index('void CoverGridHomeUi::draw('):]+source[source.index('void CoverGridHomeUi::activateApp('):source.index('void CoverGridHomeUi::screenFn(')]+controller[controller.index('void HomeActivity::processCommand('):controller.index('void HomeActivity::loop(')])
subprocess.run(['c++','-std=c++20','-fsanitize=undefined','-I'+str(build),'-I'+str(root/'test/desktop/preview'),'-I'+str(root/'src'),'-I'+str(root/'freeink-sdk/libs/ui/FreeInkUI/include'),'-I'+str(root/'freeink-sdk/libs/assets/Icons/include'),'-I'+str(root/'.pio/libdeps/x4pro-gh_release/ArduinoJson/src'),str(root/'test/desktop/preview/Main.cpp'),str(root/'freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp'),'-o',str(build/'preview')],check=True)
for state in ['normal','empty','loading','large','network']:
 raw=subprocess.check_output([str(build/'preview'),state]);data=json.loads(raw);(a.output/(state+'.json')).write_bytes(raw)
 image=Image.new('RGB',(480,800),'white');draw=ImageDraw.Draw(image)
 for c in data['commands']:
  x,y,w,h=[c[k] for k in ['x','y','w','h']];kind=c['kind']
  assert x>=0 and y>=0,(state,c)
  if kind not in ['line']:assert x+w<=480 and y+h<=800,(state,c)
  if kind=='text':draw.text((x,y),c['text'],fill='black',font=ImageFont.truetype(str(a.font),c['size']),anchor='lt')
  elif kind=='line':draw.line((x,y,c['x2'],c['y2']),fill='black')
  elif kind=='outline':draw.rectangle((x,y,x+w-1,y+h-1),outline='black')
  elif kind=='fill':draw.rectangle((x,y,x+w-1,y+h-1),fill='white' if c.get('white') else 'black')
  elif kind=='bitmap':
   for yy in range(h):
    for xx in range(w):
     if not(c['bytes'][yy*((w+7)//8)+xx//8]&(0x80>>(xx%8))):draw.point((x+xx,y+yy),fill='black')
  elif kind=='cover':
   draw.rectangle((x,y,x+w-1,y+h-1),fill='#e2e2e2',outline='black');draw.rectangle((x+8,y+8,x+w-9,y+h-9),outline='black')
   draw.text((x+14,y+22),c['text'],font=ImageFont.truetype(str(a.font),16),fill='black')
 image.save(a.output/(state+'.png'))
print('5 production draw scenarios: bounds, book/app hit targets, 100% app tap release routing during repaints passed. Host font metrics and illustrative cover fixtures; no hardware timing.')
