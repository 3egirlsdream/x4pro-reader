#!/usr/bin/env python3
"""Test the production vector adapter and page-cache lifecycle with actual streamed Chinese font bytes."""
from pathlib import Path
import argparse, subprocess, shutil

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--font', type=Path, required=True)
a = p.parse_args()
out = root/'build/reading-performance'
out.mkdir(parents=True, exist_ok=True)
sdk = root/'freeink-sdk/libs/font/FreeInkFont'
stubs = out/'stubs'
stubs.mkdir(exist_ok=True)
for name in ['Logging.h','SdCardFont.h','FontDecompressor.h']:
    shutil.copyfile(root/'test/font_cache_manager/stubs'/name, stubs/name)
(stubs/'esp_heap_caps.h').write_text('''#pragma once
#include <cstddef>
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_INTERNAL=2,MALLOC_CAP_8BIT=4;
inline size_t heap_caps_get_largest_free_block(int){return 16*1024*1024;}
inline size_t heap_caps_get_free_size(int){return 16*1024*1024;}
''')
(stubs/'MemoryManager.h').write_text('''#pragma once
#include <cstddef>
namespace freeink {struct MemoryManager {static MemoryManager&instance(){static MemoryManager m;return m;} bool ensureFree(size_t){return true;}};}
''')
includes = ['-I'+str(stubs),'-I'+str(root/'lib/EpdFont'),'-I'+str(root/'lib/Utf8'),'-I'+str(root/'lib/GfxRenderer'),'-I'+str(sdk/'include'),'-I'+str(sdk/'third_party/freetype/include')]
flags = ['-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-DFREEINK_FONT_ENABLE_AUTOHINT=1','-DBOARD_HAS_PSRAM=1']
objects = []
for source in sorted((sdk/'src/freetype').glob('*.c')):
    obj = out/(source.stem+'.o')
    subprocess.run(['cc','-std=c11',*flags,*includes,'-c',str(source),'-o',str(obj)],check=True)
    objects.append(str(obj))
sources = [root/'lib/EpdFont'/n for n in ['TtfEpdFont.cpp','EpdFont.cpp','EpdFontFamily.cpp']]
sources += [root/'lib/Utf8/Utf8.cpp',root/'lib/GfxRenderer/FontCacheManager.cpp',root/'test/reading_performance/FontCacheTest.cpp']
sources += [sdk/'src'/n for n in ['FtFont.cpp','Gsub.cpp','Gpos.cpp','FontAlloc.c']]
subprocess.run(['c++','-std=c++20',*flags,*includes,*map(str,sources),*objects,'-o',str(out/'font-test')],check=True)
subprocess.run([str(out/'font-test'),str(a.font.resolve())],check=True,timeout=60)
