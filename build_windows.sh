#!/bin/sh
# Compile Gouffre.exe pour Windows 64 bits (depuis Linux avec mingw-w64)
set -e
cd "$(dirname "$0")"
mkdir -p build
x86_64-w64-mingw32-windres res/gouffre.rc -O coff -o build/gouffre_res.o
x86_64-w64-mingw32-gcc -std=c11 -O2 -s -mwindows -static \
  -Wall -Wno-misleading-indentation -Wno-format-truncation \
  src/gfx.c src/font.c src/art.c src/data.c src/audio.c src/run.c src/run_draw.c \
  src/ui.c src/menus.c src/game.c src/meta.c src/gpu.c src/scenery.c src/cine.c src/portrait.c src/gothic.c src/screens.c src/runui.c src/voxel.c src/vmodels.c src/batiments.c src/icongen.c src/icons.c src/entrainement.c src/hdpack.c src/curios.c src/aide.c src/win32_main.c src/win32_pads.c build/gouffre_res.o \
  -o build/Gouffre.exe -lopengl32 -lgdi32 -lwinmm -luser32 -ldinput8 -ldxguid -lhid -lsetupapi -lm
echo "OK : build/Gouffre.exe"
