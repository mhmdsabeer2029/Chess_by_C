# Chess Pro -- build system
# ---------------------------
# Default target (Linux):           make
# Windows cross-compile (mingw):    make windows
# Clean:                            make clean

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g `sdl2-config --cflags` -Iinclude
LDFLAGS = `sdl2-config --libs` -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm

SRC = src/main.c src/file.c src/board.c
OUT = chess

# ---- Windows cross-compile (mingw-w64) ----
# Point WIN_SDL_PREFIX at the unpacked SDL2/SDL2_image/SDL2_ttf/SDL2_mixer
# mingw development packages, e.g. /home/you/sdl_mingw
WIN_CC          ?= x86_64-w64-mingw32-gcc
WIN_TARGET      ?= x86_64-w64-mingw32
WIN_SDL_PREFIX  ?= /home/ubuntu/sdl_mingw
WIN_SDL_INCS    = \
    -I$(WIN_SDL_PREFIX)/SDL2-2.30.10/$(WIN_TARGET)/include \
    -I$(WIN_SDL_PREFIX)/SDL2-2.30.10/$(WIN_TARGET)/include/SDL2 \
    -I$(WIN_SDL_PREFIX)/SDL2_image-2.8.4/$(WIN_TARGET)/include \
    -I$(WIN_SDL_PREFIX)/SDL2_image-2.8.4/$(WIN_TARGET)/include/SDL2 \
    -I$(WIN_SDL_PREFIX)/SDL2_ttf-2.22.0/$(WIN_TARGET)/include \
    -I$(WIN_SDL_PREFIX)/SDL2_ttf-2.22.0/$(WIN_TARGET)/include/SDL2 \
    -I$(WIN_SDL_PREFIX)/SDL2_mixer-2.8.0/$(WIN_TARGET)/include \
    -I$(WIN_SDL_PREFIX)/SDL2_mixer-2.8.0/$(WIN_TARGET)/include/SDL2
WIN_SDL_LIBS    = \
    -L$(WIN_SDL_PREFIX)/SDL2-2.30.10/$(WIN_TARGET)/lib \
    -L$(WIN_SDL_PREFIX)/SDL2_image-2.8.4/$(WIN_TARGET)/lib \
    -L$(WIN_SDL_PREFIX)/SDL2_ttf-2.22.0/$(WIN_TARGET)/lib \
    -L$(WIN_SDL_PREFIX)/SDL2_mixer-2.8.0/$(WIN_TARGET)/lib \
    -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm
WIN_CFLAGS  = -Wall -Wextra -O2 -mwindows -Iinclude $(WIN_SDL_INCS) -DSDL_MAIN_HANDLED=0

WIN_DLL_DIRS = \
    $(WIN_SDL_PREFIX)/SDL2-2.30.10/$(WIN_TARGET)/bin \
    $(WIN_SDL_PREFIX)/SDL2_image-2.8.4/$(WIN_TARGET)/bin \
    $(WIN_SDL_PREFIX)/SDL2_ttf-2.22.0/$(WIN_TARGET)/bin \
    $(WIN_SDL_PREFIX)/SDL2_mixer-2.8.0/$(WIN_TARGET)/bin

WIN_OUT_DIR = dist/ChessPro-Windows
WIN_BIN     = $(WIN_OUT_DIR)/ChessPro.exe

all: $(OUT)

$(OUT): $(SRC) include/board.h
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

# Cross-compile to a standalone Windows folder containing the .exe,
# all required DLLs and the assets/ directory.
windows: $(WIN_BIN)

$(WIN_BIN): $(SRC) include/board.h
	mkdir -p $(WIN_OUT_DIR)
	$(WIN_CC) $(WIN_CFLAGS) $(SRC) -o $(WIN_BIN) $(WIN_SDL_LIBS)
	@echo "Copying SDL2 DLLs..."
	@for d in $(WIN_DLL_DIRS); do \
	    cp -u "$$d"/*.dll $(WIN_OUT_DIR)/ 2>/dev/null || true; \
	done
	@echo "Copying assets..."
	@cp -r assets $(WIN_OUT_DIR)/
	@mkdir -p $(WIN_OUT_DIR)/Saved_Games
	@echo "Stripping debug symbols to shrink the bundle..."
	@x86_64-w64-mingw32-strip $(WIN_OUT_DIR)/ChessPro.exe $(WIN_OUT_DIR)/*.dll 2>/dev/null || true
	@echo "Done. Standalone bundle is in $(WIN_OUT_DIR)/"

# Pack the Windows bundle into a single zip the user can download / share.
windows-zip: $(WIN_BIN)
	cd dist && zip -qr ChessPro-Windows.zip ChessPro-Windows
	@echo "Created dist/ChessPro-Windows.zip"

clean:
	rm -f $(OUT)
	rm -rf dist
