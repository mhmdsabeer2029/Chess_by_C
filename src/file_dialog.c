#include "file_dialog.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>

static int win_dialog(int save_mode, const char *title,
                      const char *default_name,
                      char *out, size_t out_size) {
    OPENFILENAMEA ofn;
    char filename[1024] = {0};
    if (default_name && *default_name) {
        snprintf(filename, sizeof(filename), "%s", default_name);
    }
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = NULL;
    ofn.lpstrFilter = "Chess save (*.fen)\0*.fen\0All files\0*.*\0";
    ofn.lpstrFile   = filename;
    ofn.nMaxFile    = sizeof(filename);
    ofn.lpstrTitle  = title;
    ofn.lpstrDefExt = "fen";
    if (save_mode) {
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
        if (!GetSaveFileNameA(&ofn)) return 0;
    } else {
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
        if (!GetOpenFileNameA(&ofn)) return 0;
    }
    snprintf(out, out_size, "%s", filename);
    return out[0] != '\0';
}

int file_dialog_save(const char *title, const char *default_name,
                     char *out, size_t out_size) {
    return win_dialog(1, title ? title : "Save Game",
                      default_name, out, out_size);
}

int file_dialog_open(const char *title, char *out, size_t out_size) {
    return win_dialog(0, title ? title : "Open Game", NULL, out, out_size);
}

#else /* POSIX (Linux / macOS) */

static int run_picker(const char *cmd, char *out, size_t out_size) {
    FILE *p = popen(cmd, "r");
    if (!p) return 0;
    if (!fgets(out, (int)out_size, p)) {
        pclose(p);
        out[0] = '\0';
        return 0;
    }
    pclose(p);
    /* Strip trailing newline. */
    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) {
        out[--n] = '\0';
    }
    return out[0] != '\0';
}

static int has_tool(const char *tool) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "command -v %s >/dev/null 2>&1", tool);
    return system(cmd) == 0;
}

int file_dialog_save(const char *title, const char *default_name,
                     char *out, size_t out_size) {
    char cmd[2048];
    const char *t = title ? title : "Save Chess Game";
    const char *d = (default_name && *default_name) ? default_name : "game.fen";
    if (has_tool("zenity")) {
        snprintf(cmd, sizeof(cmd),
                 "zenity --file-selection --save --confirm-overwrite "
                 "--title=\"%s\" --filename=\"%s\" "
                 "--file-filter='Chess save | *.fen' "
                 "--file-filter='All files | *' 2>/dev/null",
                 t, d);
        return run_picker(cmd, out, out_size);
    }
    if (has_tool("kdialog")) {
        snprintf(cmd, sizeof(cmd),
                 "kdialog --getsavefilename \"%s\" \"*.fen|Chess save\" "
                 "--title \"%s\" 2>/dev/null", d, t);
        return run_picker(cmd, out, out_size);
    }
    /* Last-resort: ask on stdout. */
    fprintf(stderr, "%s (enter path, ENTER to cancel): ", t);
    if (!fgets(out, (int)out_size, stdin)) return 0;
    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) {
        out[--n] = '\0';
    }
    return out[0] != '\0';
}

int file_dialog_open(const char *title, char *out, size_t out_size) {
    char cmd[2048];
    const char *t = title ? title : "Open Chess Game";
    if (has_tool("zenity")) {
        snprintf(cmd, sizeof(cmd),
                 "zenity --file-selection --title=\"%s\" "
                 "--file-filter='Chess save | *.fen' "
                 "--file-filter='All files | *' 2>/dev/null", t);
        return run_picker(cmd, out, out_size);
    }
    if (has_tool("kdialog")) {
        snprintf(cmd, sizeof(cmd),
                 "kdialog --getopenfilename . \"*.fen|Chess save\" "
                 "--title \"%s\" 2>/dev/null", t);
        return run_picker(cmd, out, out_size);
    }
    fprintf(stderr, "%s (enter path, ENTER to cancel): ", t);
    if (!fgets(out, (int)out_size, stdin)) return 0;
    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) {
        out[--n] = '\0';
    }
    return out[0] != '\0';
}

#endif
