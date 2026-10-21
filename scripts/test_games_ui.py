#!/usr/bin/env python3
"""Exercise production activities using fake input/display/HAL; render host-only previews."""
from pathlib import Path
import argparse,json,os,subprocess
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,default=root/'build/game-preview')
parser.add_argument('--font',type=Path)
args=parser.parse_args()
out=root/'build/game-ui-tests';out.mkdir(parents=True,exist_ok=True)
langs={}
for code,name in [('en','english'),('zh','chinese-simplified')]:
 langs[code]={}
 for line in (root/'lib/I18n/translations'/f'{name}.yaml').read_text().splitlines():
  if line.startswith('STR_'):
   key,value=line.split(':',1);langs[code][key]=json.loads(value.strip())
keys=['STR_BACK']+sorted(k for k in langs['en'] if k.startswith('STR_GAME_'))
header=['#pragma once','#include <cstring>','inline bool previewChinese=true;','inline const char* previewTr(const char* key) {']
for key in keys:
 header.append('if(!strcmp(key,%s))return previewChinese?%s:%s;'%(json.dumps(key),json.dumps(langs['zh'][key],ensure_ascii=False),json.dumps(langs['en'][key])))
header+=['return key;}','#define tr(id) previewTr(#id)','enum class StrId { STR_GAME_2048, STR_GAME_GOMOKU, STR_GAME_MINES };','struct PreviewI18n { const char* get(StrId id) {return id==StrId::STR_GAME_2048?tr(STR_GAME_2048):id==StrId::STR_GAME_GOMOKU?tr(STR_GAME_GOMOKU):tr(STR_GAME_MINES);} };','inline PreviewI18n I18N;']
(out/'I18n.h').write_text('\n'.join(header)+'\n')
command=[os.environ.get('CXX','c++'),'-std=c++20','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O2','-g','-fsanitize='+os.environ.get('SANITIZERS','undefined'),
 '-I'+str(out),'-I'+str(root/'test/games/ui_stubs'),'-I'+str(root/'test/weather/stubs'),'-I'+str(root/'src'),'-I'+str(root/'lib/Memory'),'-I'+str(root/'.pio/libdeps/x4pro-gh_release/ArduinoJson/src')]
command += [str(root/'src/apps/games'/f) for f in ['GameActivity.cpp','Game2048.cpp','Gomoku.cpp','Minesweeper.cpp','MinesweeperActivity.cpp','GameSave.cpp']]
command += [str(root/'src/apps/AppStorage.cpp')]
for source, executable in [('GameUiTest.cpp','ui-tests'),('MinesweeperUiTest.cpp','mines-ui-tests')]:
 subprocess.run(command+[str(root/'test/games'/source),'-o',str(out/executable)],check=True)
 for lang in ['zh','en']:
  subprocess.run([str(out/executable),lang,str(args.output/lang)],check=True,timeout=60)
if args.font:
 from PIL import Image,ImageDraw,ImageFont
 failures=[]
 for file in sorted(args.output.glob('*/*.json')):
  image=Image.new('RGB',(480,800),'white');draw=ImageDraw.Draw(image)
  for c in json.loads(file.read_text())['commands']:
   x,y=c['x'],c['y'];black='black' if c.get('black',True) else 'white'
   if c['op']=='text':
    font=ImageFont.truetype(str(args.font),c['size']);box=draw.textbbox((x,y),c['text'],font=font,anchor='lt')
    if box[0]<0 or box[1]<0 or box[2]>480 or box[3]>800:failures.append((str(file),c['text'],box))
    draw.text((x,y),c['text'],font=font,fill='black',anchor='lt')
   elif c['op']=='line':draw.line((x,y,c['x2'],c['y2']),fill=black)
   elif c['op']=='pixel':draw.point((x,y),fill=black)
   else:
    box=(x,y,x+c['w']-1,y+c['h']-1)
    if min(x,y)<0 or box[2]>=480 or box[3]>=800:failures.append((str(file),c))
    if c['op']=='outline':draw.rectangle(box,outline=black,width=c['width'])
    else:draw.rectangle(box,fill=black)
  image.save(file.with_suffix('.png'))
 if failures: raise SystemExit('Bounds failures: '+str(failures))
 print(f'PASS {len(list(args.output.glob("*/*.json")))} production-render host previews in Chinese/English; all text/board bounds checked (HAL/fonts are simulated)')
