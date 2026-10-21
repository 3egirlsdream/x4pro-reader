#!/usr/bin/env python3
"""Make a static glyf-based SD font; requires fonttools==4.67.0."""
import argparse
import hashlib
import json
from pathlib import Path

from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('license', type=Path)
p.add_argument('output', type=Path)
args = p.parse_args()
font = TTFont(args.source, recalcTimestamp=False)
assert 'glyf' in font and 'CFF ' not in font
font = instantiateVariableFont(font, {'wght': 400}, inplace=True)
# Rename the derivative while retaining original copyright and license records.
names = {1: 'X4Pro Sans SC', 2: 'Regular', 3: 'X4ProSansSC-Regular-1.0',
         4: 'X4Pro Sans SC Regular', 6: 'X4ProSansSC-Regular',
         16: 'X4Pro Sans SC', 17: 'Regular'}
for record in list(font['name'].names):
    if record.nameID in names:
        font['name'].setName(names[record.nameID], record.nameID,
                             record.platformID, record.platEncID, record.langID)
args.output.mkdir(parents=True, exist_ok=True)
target = args.output / 'X4ProSansSC-Regular.ttf'
font.save(target)
(args.output / 'LICENSE.txt').write_bytes(args.license.read_bytes())
probe = '中文阅读天气北京上海温度湿度风速更新时间暂无天气计数器'
assert all(ord(c) in font.getBestCmap() for c in probe)
metadata = {'source': 'https://github.com/google/fonts/tree/main/ofl/notosanssc',
            'sourceFile': 'NotoSansSC[wght].ttf',
            'sourceSha256': hashlib.sha256(args.source.read_bytes()).hexdigest(),
            'transform': 'fonttools 4.67.0; instantiate wght=400; rename family X4Pro Sans SC',
            'output': target.name, 'sha256': hashlib.sha256(target.read_bytes()).hexdigest(),
            'bytes': target.stat().st_size, 'cmapCodepoints': len(font.getBestCmap()),
            'outline': 'TrueType glyf; no CFF; no variable axes',
            'license': 'SIL Open Font License 1.1; see LICENSE.txt'}
(args.output / 'SOURCE.json').write_text(json.dumps(metadata, indent=2) + '\n')
print(json.dumps(metadata, indent=2))
