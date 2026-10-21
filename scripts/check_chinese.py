#!/usr/bin/env python3
"""Validate Chinese coverage, format parameters, duplicate keys and optional font glyph coverage."""
import argparse
import json
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--font', type=Path)
args = p.parse_args()
def read(name):
    result = {}
    for line in (root/'lib/I18n/translations'/name).read_text().splitlines():
        if not line.startswith('STR_'): continue
        key, value = line.split(':', 1)
        assert key not in result, 'Duplicate key: '+key
        result[key] = json.loads(value.strip())
    return result
english, chinese = read('english.yaml'), read('chinese-simplified.yaml')
assert english.keys() == chinese.keys(), 'Missing/extra translation keys'
pattern = r'%(?:\d+\$)?[-+ #0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:hh|ll|[hljztL])?[diuoxXfFeEgGaAcspn%]'
for key in english:
    assert re.findall(pattern, english[key]) == re.findall(pattern, chinese[key]), 'Format mismatch: '+key
if args.font:
    from fontTools.ttLib import TTFont
    cmap = TTFont(args.font).getBestCmap()
    required = {ord(c) for text in chinese.values() for c in text if ord(c) >= 0x3000}
    missing = required-set(cmap)
    assert not missing, 'Missing font glyphs: '+repr(sorted(missing))
print(f'{len(chinese)} Chinese keys: complete coverage; format parameters and supplied font glyphs passed')
