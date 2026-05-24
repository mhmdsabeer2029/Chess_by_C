#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

#include <stddef.h>

/* Native OS file pickers.
 *
 * On Windows: uses GetSaveFileName / GetOpenFileName (commdlg.h).
 * On Linux:   uses `zenity --file-selection`, falling back to `kdialog`,
 *             then a typed prompt in stdout if neither is installed.
 *
 * Returns 1 on success and writes the chosen path into `out` (NUL-terminated).
 * Returns 0 if the user cancelled or no dialog tool was available. */
int file_dialog_save(const char *title, const char *default_name,
                     char *out, size_t out_size);

int file_dialog_open(const char *title,
                     char *out, size_t out_size);

#endif
