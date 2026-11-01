#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
command -v emcc >/dev/null || { echo 'Emscripten not installed. Run: source /path/to/emsdk/emsdk_env.sh'; exit 1; }
mkdir -p site
mapfile -t GAME_SOURCES < <(find src snes -maxdepth 1 -name '*.c' ! -name 'opengl.c' ! -name 'glsl_shader.c' | sort)
GAME_SOURCES+=(third_party/opus-1.3.1-stripped/opus_decoder_amalgam.c)
# Link the original modified game core, including the new Dark World story and ray tracer.
emcc "${GAME_SOURCES[@]}" -I . -O2 \
  -DSYSTEM_VOLUME_MIXER_AVAILABLE=0 \
  --use-port=sdl2 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sINITIAL_MEMORY=100663296 \
  -sMAXIMUM_MEMORY=536870912 \
  -sFORCE_FILESYSTEM=1 \
  -sINVOKE_RUN=0 \
  -sENVIRONMENT=web \
  -sEXPORTED_FUNCTIONS=_main,_WebButton,_WebSave,_WebLoad,_WebToggleLight,_WebCycleLight,_WebFPS,_WebPause \
  -sEXPORTED_RUNTIME_METHODS=FS,callMain \
  -sDISABLE_EXCEPTION_CATCHING=1 \
  -lidbfs.js \
  --pre-js web/preload.js \
  --preload-file web/zelda3_assets.dat@/zelda3_assets.dat \
  --preload-file web/zelda3.ini@/zelda3.ini \
  -o site/game.js -lm
printf '\nBuilt site/game.js, site/game.wasm, site/game.data\n'
