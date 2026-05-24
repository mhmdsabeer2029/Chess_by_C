# ♟️ Professional Chess Engine (C & SDL2)

![Alexandria University](https://img.shields.io/badge/University-Alexandria-blue)
![Faculty](https://img.shields.io/badge/Faculty-Engineering-green)
![Course](https://img.shields.io/badge/Course-CSE212:_Programming_I-red)

A robust, cross-platform Chess application representing a sophisticated fusion of a high-performance logical engine and a modern graphical interface. Engineered from the ground up to ensure 100% FIDE rule compliance.

---

## 👥 Prepared By
* **Mohamed Saber Abaas Elsayed** (ID: 24010617)
* **Fares Tahseen Mohamed Nageeb** (ID: 24010493)
* *Under the supervision of Alexandria University, Faculty of Engineering.*

---

## 🚀 Key Technical Features

### 1. 🧠 Core Logic & Game Physics (board.c)
* **FIDE Compliance:** Full implementation of **Castling**, **En Passant**, and **Pawn Promotion**.
* **Movement Engine:** Uses a **Directional Vector Algorithm** for sliding pieces and pre-computed offset arrays for Knights.
* **Virtual Simulation:** A "Look-ahead" layer that clones the board using `malloc` to validate moves and prevent illegal King exposure (Pinned pieces logic).
* **State Detection:** Advanced logic for **Checkmate**, **Stalemate**, **Insufficient Material**, and **Threefold Repetition**.

### 2. 🎨 Graphical User Interface (main.c & SDL2) -- *chess.com inspired*
* **Modern Board Theme:** Cream/green colour palette modelled on chess.com.
* **Visual Feedback:**
    * Yellow last-move highlight
    * Yellow-green selected piece highlight
    * Grey **dots for empty target squares** and **rings for capturable pieces**
    * Red overlay on the king when it is in check
* **Promotion Popup:** Click the icon for Queen / Rook / Bishop / Knight (keyboard `Q/R/B/N` also work).
* **Move History Panel:** Live SAN-style algebraic notation with auto-scroll.
* **Captured pieces & material score:** Rendered above/below the board with a `+N` advantage indicator.
* **Flip Board, Undo / Redo, Resign, Offer Draw** buttons.
* **Graphical Load Dialog:** Choose any previously saved game from a clickable list.
* **Auditory Experience:** Integrated `SDL_mixer` for distinct acoustic signatures (moves, checks, captures, illegal moves) -- gracefully disabled when no audio device is available.

### 3. 💾 Persistence & Standardization (file.c)
* **FEN Engine:** Complete support for **Forsyth-Edwards Notation** (FEN) for game serialization.
* **Secure Storage:** Robust I/O system with automated naming to prevent data overwriting and syntax validation for loaded files.

---

## 🛠️ Data Architecture
The engine is built on a modular three-layer architecture as defined in `board.h`:
* **Atomic Layer:** Individual piece history and state tracking (`has_moved`).
* **Entity Layer:** Player-specific data and cached King positions for $O(1)$ check detection.
* **Global Layer:** The "Single Source of Truth" board state, synchronizing the logical grid with the SDL2 graphical grid.

---

## ⚙️ Installation & How to Run

This project uses a structured **GNU Makefile** for automated compilation.

### Linux

```bash
# 1. Clean previous builds
make clean

# 2. Compile the project
make

# 3. Run the engine
./chess
```

### Windows (standalone `.exe`)

The recommended way to ship the game to a Windows user is the bundled folder produced by:

```bash
# Cross-compile from a Linux host with mingw-w64 + SDL2 mingw dev libraries
make windows           # builds dist/ChessPro-Windows/{ChessPro.exe + DLLs + assets}
make windows-zip       # also produces dist/ChessPro-Windows.zip (~2.5 MB)
```

The `dist/ChessPro-Windows/` folder is **fully self-contained** -- copy it to any Windows machine and double-click `ChessPro.exe`. No Visual Studio runtime, MSYS, or separate SDL install is required.

Inputs that the cross-build expects (override on the command line if your paths differ):

| Variable          | Default                                |
|-------------------|----------------------------------------|
| `WIN_CC`          | `x86_64-w64-mingw32-gcc`               |
| `WIN_SDL_PREFIX`  | `/home/ubuntu/sdl_mingw` (containing the unpacked SDL2 / SDL2_image / SDL2_ttf / SDL2_mixer mingw dev packages) |

### Native Windows build

If you would rather build directly on Windows under MSYS2:

```bash
pacman -S --noconfirm mingw-w64-x86_64-{gcc,SDL2,SDL2_image,SDL2_ttf,SDL2_mixer} make
make CC=gcc CFLAGS="-Wall -Wextra -O2 -Iinclude" \
     LDFLAGS="-lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm"
```

---

> ⌨️ Keyboard shortcuts: `Q / R / B / N` choose a promotion piece; `Esc` closes the Load dialog.
