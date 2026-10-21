#!/usr/bin/env python3
"""Build and run production offline game models and JSON/SD persistence on the host."""
from pathlib import Path
import os
import platform
import subprocess
root=Path(__file__).resolve().parents[1]
out=root/'build/game-tests'
out.mkdir(parents=True,exist_ok=True)
common=[os.environ.get('CXX','c++'),'-std=c++20','-Wall','-Wextra','-Werror','-O2','-g',
        '-fsanitize='+os.environ.get('SANITIZERS','undefined' if platform.system()=='Darwin' else 'address,undefined'),'-I'+str(root/'src')]
models=[str(root/'src/apps/games'/name) for name in ['Game2048.cpp','Gomoku.cpp','Minesweeper.cpp']]
def run(extra,test,name):
    command=common+extra+models+[str(root/'test/games'/test),'-o',str(out/name)]
    subprocess.run(command,check=True)
    subprocess.run([str(out/name)],check=True,timeout=60)
run([],'GameTest.cpp','game-tests')
run(['-I'+str(root/'.pio/libdeps/x4pro-gh_release/ArduinoJson/src'),'-I'+str(root/'test/weather/stubs'),'-I'+str(root/'lib/Memory'),
     str(root/'src/apps/AppStorage.cpp'),str(root/'src/apps/games/GameSave.cpp')],'GameSaveTest.cpp','save-tests')

run([], 'MinesweeperTest.cpp', 'mines-tests')
run(['-I'+str(root/'.pio/libdeps/x4pro-gh_release/ArduinoJson/src'),'-I'+str(root/'test/weather/stubs'),'-I'+str(root/'lib/Memory'),
     str(root/'src/apps/AppStorage.cpp'),str(root/'src/apps/games/GameSave.cpp')], 'MinesweeperSaveTest.cpp', 'mines-save-tests')
