#pragma once
/** @file Abstraction to Microsoft's FileOpenDialog
 */
#ifndef QAGEN_FILE_DIALOG_H
#define QAGEN_FILE_DIALOG_H

#include "qagen-defs.h"
#include <ShlObj.h>


/** This struct is MT/AS-Unsafe */
struct qagen_filedlg {
    IFileOpenDialog *fd;

    DWORD            count;
    IShellItemArray *siarr;
    IShellItem      *si;    /* I'm fairly certain that this needs to remain in
                            scope for its strings to be valid */
};


enum {
    FILEDLG_CLOSED = -1,    /* The user closed the window */
    FILEDLG_NORMAL =  0,    /* The user made a selection */
    FILEDLG_ERROR  =  1     /* An error occurred */
};


/** @brief Creates and opens the file dialog
 *  @param fdlg
 *      Pointer to file dialog context. This is immediately memset to zero
 *  @returns A FILEDLG_* constant
 */
int qagen_filedlg_show(struct qagen_filedlg *fdlg);


/** @brief Destroys/releases all resources held by @p fdlg. This is always safe
 *      to call (as long as you zero initialized the struct)
 *  @param fdlg
 *      File dialog context
 */
void qagen_filedlg_destroy(struct qagen_filedlg *fdlg);


/** @brief Gets the @p idx -indexed filesystem path and display name from the
 *      file dialog. This operation WILL invalidate the previous strings that
 *      it retrieved, irrespective of whether it succeeds or not
 *  @param fdlg
 *      File dialog context
 *  @param idx
 *      Index of the selected path
 *  @param path
 *      Address where the pointer to the fully qualified path string will be
 *      written
 *  @param dpy
 *      Address where the pointer to the display name will be written
 *  @returns Nonzero on error
 */
int qagen_filedlg_path(struct qagen_filedlg *fdlg,
                       DWORD                 idx,
                       wchar_t    *restrict *path,
                       wchar_t    *restrict *dpy);


#endif /* QAGEN_FILE_DIALOG_H */
