#!/usr/bin/env python3
"""Load and rasterize an SD font using the pinned SDK's curated FreeType modules."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('font', type=Path)
args = p.parse_args()
root = Path(__file__).resolve().parents[1]
sdk = root / 'freeink-sdk/libs/font/FreeInkFont'
out = root / 'build/font-check'
out.mkdir(parents=True, exist_ok=True)
source = out / 'check.c'
source.write_text(r'''
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
int main(int argc, char **argv) {
  FT_Library library;
  FT_Face face;
  if (argc != 2 || FT_Init_FreeType(&library)) return 1;
  FT_Error error = FT_New_Face(library, argv[1], 0, &face);
  if (error) { fprintf(stderr, "FT_New_Face error=%d\n", error); return 2; }
  const FT_ULong probe[] = {0x4e2d,0x6587,0x9605,0x8bfb,0x5929,0x6c14,0x5317,0x4eac,0x6e29,0x5ea6};
  const unsigned sizes[] = {8,10,12,14,16,18,32};
  for (unsigned s=0; s<sizeof(sizes)/sizeof(*sizes); ++s) {
    if (FT_Set_Pixel_Sizes(face, 0, sizes[s])) return 3;
    for (unsigned i=0; i<sizeof(probe)/sizeof(*probe); ++i) {
      if (!FT_Get_Char_Index(face, probe[i]) ||
          FT_Load_Char(face, probe[i], FT_LOAD_RENDER) ||
          !face->glyph->bitmap.width || !face->glyph->bitmap.rows) return 4;
    }
  }
  printf("SDK curated FreeType: %s; 70 Chinese glyph/size rasterizations passed\n", face->family_name);
  FT_Done_Face(face);
  FT_Done_FreeType(library);
  return 0;
}
''')
modules = ['ft_ftbase', 'ft_ftbitmap', 'ft_ftdebug', 'ft_ftglyph', 'ft_ftinit',
           'ft_ftmm', 'ft_ftsystem', 'ft_sfnt', 'ft_smooth', 'ft_truetype']
subprocess.run(['cc', '-std=c99', '-O1', '-I'+str(sdk/'third_party/freetype/include'),
                '-I'+str(sdk/'include'), str(source), str(sdk/'src/FontAlloc.c'),
                *[str(sdk/'src/freetype'/(m+'.c')) for m in modules],
                '-o', str(out/'check')], check=True)
subprocess.run([str(out/'check'), str(args.font.resolve())], check=True)
