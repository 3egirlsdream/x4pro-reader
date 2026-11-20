#!/usr/bin/env python3
"""Run host tests against production weather parsing and power policy code."""
from pathlib import Path
import os
import platform
import subprocess

root = Path(__file__).resolve().parents[1]
json_headers = root / '.pio/libdeps/x4pro-gh_release/ArduinoJson/src'
if not json_headers.exists():
    raise SystemExit('Install the x4pro-gh_release PlatformIO dependencies first.')
out = root / 'build/weather-tests'
out.mkdir(parents=True, exist_ok=True)
sanitizers = os.environ.get('SANITIZERS', 'undefined' if platform.system() == 'Darwin' else 'address,undefined')
command = [os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
           '-fsanitize=' + sanitizers, '-g', '-I' + str(root / 'src'), '-I' + str(json_headers),
           str(root / 'src/weather/WeatherModel.cpp'), str(root / 'src/weather/WeatherJson.cpp'),
           str(root / 'src/weather/WeatherCities.cpp'), str(root / 'test/weather/WeatherTest.cpp'),
           '-o', str(out / 'weather-tests')]
subprocess.run(command, check=True)
subprocess.run([str(out / 'weather-tests'), str(root / 'test/weather/fixtures/open-meteo-beijing.json')],
               check=True, timeout=30)
storage_command = command[:command.index(str(root / 'src/weather/WeatherModel.cpp'))]
storage_command.extend(['-I' + str(root / 'test/weather/stubs'), '-I' + str(root / 'lib/Memory'),
                        str(root / 'src/apps/AppStorage.cpp'), str(root / 'test/weather/AppStorageTest.cpp'),
                        '-o', str(out / 'app-storage-tests')])
subprocess.run(storage_command, check=True)
subprocess.run([str(out / 'app-storage-tests')], check=True, timeout=30)
store_command = command[:command.index(str(root / 'src/weather/WeatherModel.cpp'))]
store_command.insert(1, '-I' + str(root / 'test/weather/stubs'))
store_command.extend(['-I' + str(root / 'lib/Memory'),
                      str(root / 'src/apps/AppStorage.cpp'), str(root / 'src/weather/WeatherStore.cpp'),
                      str(root / 'src/weather/WeatherModel.cpp'), str(root / 'src/weather/WeatherJson.cpp'),
                      str(root / 'src/weather/WeatherCities.cpp'), str(root / 'test/weather/WeatherStoreTest.cpp'),
                      '-o', str(out / 'weather-store-tests')])
subprocess.run(store_command, check=True)
subprocess.run([str(out / 'weather-store-tests')], check=True, timeout=30)

request_command = command[:command.index(str(root / 'src/weather/WeatherModel.cpp'))]
request_command.insert(1, '-I' + str(root / 'test/weather/request-stubs'))
request_command.extend(['-I' + str(root / 'test/weather/stubs'), '-I' + str(root / 'lib/Performance'),
                        str(root / 'src/weather/WeatherRequest.cpp'), str(root / 'src/weather/WeatherModel.cpp'),
                        str(root / 'src/weather/WeatherJson.cpp'), str(root / 'test/weather/WeatherRequestTest.cpp'),
                        '-o', str(out / 'weather-request-tests')])
subprocess.run(request_command, check=True)
subprocess.run([str(out / 'weather-request-tests'), str(root / 'test/weather/fixtures/open-meteo-beijing.json')],
               check=True, timeout=30)
