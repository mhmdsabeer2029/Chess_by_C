# Chess Pro -- build system
# ---------------------------
# Default target (Linux):           make
# Windows cross-compile (mingw):    make windows
# Windows zip bundle:               make windows-zip
# Clean:                            make clean

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g `sdl2-config --cflags` -Iinclude
LDFLAGS = `sdl2-config --libs` -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm -lpthread

SRC = src/main.c src/file.c src/board.c src/engine.c src/coach.c src/file_dialog.c
OUT = chess

# ---- Windows cross-compile (mingw-w64) ----
WIN_CC          ?= x86_64-w64-mingw32-gcc
WIN_TARGET      ?= x86_64-w64-mingw32
WIN_RC          ?= x86_64-w64-mingw32-windres
WIN_STRIP       ?= x86_64-w64-mingw32-strip
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
    -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer \
    -lm -lcomdlg32 -lole32 -luuid -lwinmm

WIN_CFLAGS  = -Wall -Wextra -O2 -mwindows -Iinclude $(WIN_SDL_INCS) -DSDL_MAIN_HANDLED=0

WIN_DLL_DIRS = \
    $(WIN_SDL_PREFIX)/SDL2-2.30.10/$(WIN_TARGET)/bin \
    $(WIN_SDL_PREFIX)/SDL2_image-2.8.4/$(WIN_TARGET)/bin \
    $(WIN_SDL_PREFIX)/SDL2_ttf-2.22.0/$(WIN_TARGET)/bin \
    $(WIN_SDL_PREFIX)/SDL2_mixer-2.8.0/$(WIN_TARGET)/bin

WIN_OUT_DIR = dist/ChessPro-Windows
WIN_BIN     = $(WIN_OUT_DIR)/ChessPro.exe
WIN_RES     = $(WIN_OUT_DIR)/chess.res

HDRS = include/board.h include/engine.h include/coach.h include/file_dialog.h

all: $(OUT)

$(OUT): $(SRC) $(HDRS)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

# Cross-compile to a standalone Windows folder containing the .exe,
# all required DLLs and the assets/ directory.
windows: $(WIN_BIN)

$(WIN_BIN): $(SRC) $(HDRS) chess.rc chess.exe.manifest
	mkdir -p $(WIN_OUT_DIR)
	@echo "Compiling Win32 resource (VERSIONINFO + manifest + icon)..."
	$(WIN_RC) -I. chess.rc -O coff -o $(WIN_RES)
	$(WIN_CC) $(WIN_CFLAGS) $(SRC) $(WIN_RES) -o $(WIN_BIN) $(WIN_SDL_LIBS)
	@echo "Copying SDL2 DLLs..."
	@for d in $(WIN_DLL_DIRS); do \
	    cp -u "$$d"/*.dll $(WIN_OUT_DIR)/ 2>/dev/null || true; \
	done
	@echo "Copying assets..."
	@cp -r assets $(WIN_OUT_DIR)/
	@mkdir -p $(WIN_OUT_DIR)/Saved_Games
	@echo "Stripping debug symbols to shrink the bundle..."
	@$(WIN_STRIP) $(WIN_OUT_DIR)/ChessPro.exe $(WIN_OUT_DIR)/*.dll 2>/dev/null || true
	@rm -f $(WIN_RES)
	@echo "Done. Standalone bundle is in $(WIN_OUT_DIR)/"

# Pack the Windows bundle into a single zip.
windows-zip: $(WIN_BIN)
	cd dist && rm -f ChessPro-Windows.zip && zip -qr ChessPro-Windows.zip ChessPro-Windows
	@echo "Created dist/ChessPro-Windows.zip"

clean:
	rm -f $(OUT)
	rm -rf dist
