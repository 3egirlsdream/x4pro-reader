# Third-party attribution for the weather extension

The reader remains derived from CrossPoint Reader 1.6.5,
commit `93e98bb78702e29868a16a13b80c40e6b36ccdff` (MIT).
Its LICENSE and vendored dependency notices remain in place.
FreeInk SDK stays pinned to `111fdcc7f0176c3ee38391a160ee296bf492dbd8`;
the Lucide submodule stays pinned to `c81680e066f45b640743ca78ae36cdedda3f0318`.
No SDK source was modified by this extension.

`src/weather/WeatherGraphics.cpp` adapts the self-contained big-digit and
primitive weather-icon drawing code from Tony Zolnoski's
https://github.com/t0nyz0/crosspoint-reader-lockscreens at
`52f6c30ecfeaf09f38dfd1e7fd8be9073b2da255`.
The original MIT license is reproduced in
`docs/weather-apps/lockscreens-MIT-LICENSE.txt`.
The timed-wake versus power-wake architecture also references that project;
its C3 power-latch code and US ZIP lookup were not transplanted.

`data/weather/cities.json` is a filtered, localized GeoNames cities15000 data
snapshot: https://download.geonames.org/export/dump/cities15000.zip .
Its archive SHA-256 is recorded in the JSON. Coordinates remain WGS84.
GeoNames data is licensed CC BY 4.0: https://www.geonames.org/about.html .
Local Chinese display labels were simplified using OpenCC; the dataset has
350 places grouped into 31 mainland provincial groups. It is a weather
location selector, not a complete current administrative-division register.

Weather data: Open-Meteo, https://open-meteo.com/ . The firmware displays
attribution. The public endpoint is used for this personal, noncommercial
project. Recheck service terms before commercial distribution.

The separately bundled X4Pro Sans SC SD-card font derives from Google Fonts'
Noto Sans SC: https://github.com/google/fonts/tree/main/ofl/notosanssc .
It is instantiated at weight 400 and renamed using fonttools 4.67.0 by
`scripts/build_x4pro_font.py`. The source SHA-256 and output SHA-256 are in
`reader-sdcard/fonts/X4ProSansSC/SOURCE.json`; SIL Open Font License 1.1 and
original copyright notices are in its LICENSE.txt. It uses TrueType glyf
outlines compatible with the pinned SDK's curated FreeType modules. The
previous CFF OTF resource was incompatible and has been replaced. The font
is not embedded into the firmware application slot.

CrossPlay, esphome-xteink, CrossDiTo, xteink-meteo-sleep and Tesserae were
reviewed as design references. Their code was not copied into this extension.
