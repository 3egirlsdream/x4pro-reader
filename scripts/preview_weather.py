#!/usr/bin/env python3
"""Render the production weather layout with a host drawing shim (not a hardware screenshot)."""
import argparse
import json
import os
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=root / 'build/weather-preview')
parser.add_argument('--font', type=Path, default=root.parent / 'reader-sdcard/fonts/X4ProSansSC/X4ProSansSC-Regular.ttf')
args = parser.parse_args()
build = root / 'build/weather-preview'
build.mkdir(parents=True, exist_ok=True)
args.output.mkdir(parents=True, exist_ok=True)
langs = {}
for code, filename in [('en','english.yaml'), ('zh','chinese-simplified.yaml')]:
    values = {}
    for line in (root/'lib/I18n/translations'/filename).read_text().splitlines():
        if line.startswith('STR_'):
            key, value = line.split(':', 1)
            values[key] = json.loads(value.strip())
    langs[code] = values
keys = sorted(k for k in langs['en'] if k.startswith(('STR_WX_', 'STR_APP_')) or k == 'STR_BACK')
header = ['#pragma once', '#include <cstring>', 'inline bool previewChinese=false;',
          'inline const char* previewTr(const char* key) {']
for key in keys:
    header.append('  if (!strcmp(key,%s)) return previewChinese ? %s : %s;' % (
        json.dumps(key), json.dumps(langs['zh'].get(key,langs['en'][key]),ensure_ascii=False),
        json.dumps(langs['en'][key],ensure_ascii=False)))
header += ['  return key;', '}', '#define tr(id) previewTr(#id)']
(build/'I18n.h').write_text('\n'.join(header)+'\n')
subprocess.run([os.environ.get('CXX','c++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
    '-I'+str(build), '-I'+str(root/'test/weather/preview_stubs'), '-I'+str(root/'src'),
    '-I'+str(root/'.pio/libdeps/x4pro-gh_release/ArduinoJson/src'),
    str(root/'src/weather/WeatherView.cpp'),str(root/'src/weather/WeatherGraphics.cpp'),
    str(root/'src/weather/WeatherModel.cpp'),str(root/'src/weather/WeatherJson.cpp'),
    str(root/'test/weather/WeatherPreview.cpp'),'-o',str(build/'preview')],check=True)
failures = []
count = 0
for lang in ['zh','en']:
    for state in ['sleep','interactive','offline','battery','empty','extreme']:
        for top in [5,10,13]:
            raw = subprocess.check_output([str(build/'preview'),str(root/'test/weather/fixtures/open-meteo-beijing.json'),lang,state,str(top)])
            commands = json.loads(raw)['commands']
            image = Image.new('RGB',(800,480),'white')
            draw = ImageDraw.Draw(image)
            for c in commands:
                x,y = c['x'],c['y']
                if c['op']=='text':
                    font=ImageFont.truetype(str(args.font),c['size'])
                    box=draw.textbbox((x,y),c['text'],font=font,anchor='lt')
                    if box[0]<0 or box[1]<0 or box[2]>800 or box[3]>480:
                        failures.append((lang,state,top,c['text'],box))
                    draw.text((x,y),c['text'],font=font,fill='black',anchor='lt')
                elif c['op']=='line':
                    draw.line((x,y,c['x2'],c['y2']),fill='black',width=c['width'])
                else:
                    box=(x,y,x+c['w']-1,y+c['h']-1)
                    if min(x,y)<0 or box[2]>=800 or box[3]>=480: failures.append((lang,state,top,c))
                    if c['op']=='rounded': draw.rounded_rectangle(box,radius=c['r'],fill='black' if c['fill'] else 'white')
                    elif c['op']=='outline': draw.rectangle(box,outline='black')
                    else: draw.rectangle(box,fill='black' if c['fill'] else 'white')
            if top==5:
                image.save(args.output/f'{lang}-{state}.png')
            count+=1
if failures:
    raise SystemExit('Layout bounds failures: '+str(failures))
print(f'{count} production-layout scenarios passed bounds checks; 12 host previews saved to {args.output}')
