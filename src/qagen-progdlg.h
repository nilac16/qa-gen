#pragma once
/** @file (Fixed) interface to ProgressDialog
 */
#ifndef QAGEN_PROGRESS_DIALOG_H
#define QAGEN_PROGRESS_DIALOG_H

#include "qagen-defs.h"
#include <ShlObj.h>


struct qagen_progdlg {
    struct {
        struct {
            IUnknownVtbl        parent;
            IProgressDialogVtbl cls;
        } *vtbl;
    } *pd;
};


/** @brief Constructs and displays the progress dialog */
int qagen_progdlg_show(struct qagen_progdlg *pdlg, const wchar_t *title);


/** @brief Releases the progress dialog and closes the window */
void qagen_progdlg_destroy(struct qagen_progdlg *pdlg);


/** @brief Sets the progress count for @p pdlg
 *  @param pdlg
 *      Progress dialog
 *  @param completed
 *      Completed progress
 *  @param total
 *      Total needed progress
 *  @returns Nonzero on error
 */
int qagen_progdlg_set_progress(struct qagen_progdlg *pdlg,
                               ULONGLONG             completed,
                               ULONGLONG             total);


/** @brief Sets the string for line @p lineno to @p line
 *  @param pdlg
 *      Progress dialog
 *  @param line
 *      String
 *  @param lineno
 *      Line number to change
 *  @returns Nonzero on error
 */
int qagen_progdlg_set_line(struct qagen_progdlg *pdlg,
                           const wchar_t        *line,
                           DWORD                 lineno);


/** @brief Returns true if the user cancelled the operation */
bool qagen_progdlg_cancelled(const struct qagen_progdlg *pdlg);


#endif /* QAGEN_PROGRESS_DIALOG_H */
