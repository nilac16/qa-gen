#pragma once

#ifndef QAGEN_COPY_H
#define QAGEN_COPY_H

#include "qagen-defs.h"
#include "qagen-patient.h"
#include "qagen-progdlg.h"

#define COPY_TITLE_LEN 81
#define COPY_LINE1_LEN 101
#define COPY_LINE2_LEN 101


/** The state of the copy operation. The progress dialog is updated from
 *  information in this record, and it is used as the user data for the
 *  CopyFileEx callback function
 */
struct qagen_copy_ctx {
    wchar_t title[COPY_TITLE_LEN];  /* This is the window caption. Set this
                                    before the operation */
    wchar_t line1[COPY_LINE1_LEN];  /* Describe the current operation: Show
                                    file x of y, (n bytes remaining). This is
                                    set by the progress routine, so when you
                                    convert MHD files, you must set this
                                    yourself */
    wchar_t line2[COPY_LINE2_LEN];  /* Show the name of the file being copied/
                                    converted. Set this while said name is in
                                    scope */

    struct qagen_progdlg pdlg;

    /** @note The progress dialog exposes no method to actually do either of
     *      these things, with the potential exception of just destroying the
     *      window to hide it
     */
    BOOL opcancel;
    BOOL hide;

    uint32_t nfiles;    /* Number of files, set before the operation, and not
                        modified during it */

    uint32_t ncopied;   /* The number of files copied. Increment this after
                        every successful copy/conversion */

    ULONGLONG curcopy;      /* Progress on the current file. Set this to zero
                            before copying any file. The progress routine
                            updates this value. Don't worry about changing this
                            after converting MHD files, it is only used to show
                            progress to the user */

    ULONGLONG completed;    /* Currently completed progress. Set this to zero
                            before starting the operation, and the progress
                            routine updates it. After converting MHD files, add
                            the size of the RD template to this and update */

    ULONGLONG total;        /* Total progress required. Set this before the
                            operation and don't modify it */

    ULONGLONG templatesz;   /* The file size of the RD template, if needed. Set
                            before the operation, and don't modify */
};


/** @brief Copies the files to the (prepared) directory */
int qagen_copy_patient(struct qagen_patient *pt);


/** @brief Updates the contained progress dialog with the current values in
 *      @p ctx
 *  @param ctx
 *      Copy context
 *  @returns Nonzero if an error occurred
 */
int qagen_copy_update_dlg(struct qagen_copy_ctx *ctx);


/** @brief Copy callback for files that can be copied.
 *      I am not documenting these args
 *      https://learn.microsoft.com/en-us/windows/win32/api/winbase/nc-winbase-lpprogress_routine
 *  @details This updates the progress of the current file
 *  @note This cannot be used for converting MHD files
 */
DWORD qagen_copy_proc(LARGE_INTEGER totalsz, LARGE_INTEGER totalxfer,
                      LARGE_INTEGER strmsz,  LARGE_INTEGER strmxfer,
                      DWORD         strmno,  DWORD         cbreason,
                      HANDLE        hsrc,    HANDLE        hdest,
                      struct qagen_copy_ctx *cctx);


#endif /* QAGEN_COPY_H */
