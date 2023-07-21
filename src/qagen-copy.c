#include <stdio.h>
#include "qagen-copy.h"
#include "qagen-error.h"
#include "qagen-string.h"
#include "qagen-metaio.h"
#include "qagen-memory.h"
#include "qagen-log.h"

/** C4100: My ears are still ringing */
#pragma warning(disable: 4100)


/** @brief Formats @p bytes into an immediately readable significand + SI
 *      prefix
 *  @details The significand is taken to be no greater than 10,000 of a
 *      particular unit
 *  @param bytes
 *      Number of bytes to format
 *  @param[out] units
 *      Pointer to address of unit string
 *  @param[out] signif
 *      Address where the formatted significand is written
 *  @note This function only contains prefixes up to and including petabytes,
 *      so... make sure you don't copy more than 10k petabytes lmao
 */
static void qagen_copy_format_bytes(LONGLONG        bytes,
                                    const wchar_t **units,
                                    int            *signif)
{
    static const wchar_t *si[] = { L"B", L"kB", L"MB", L"GB", L"TB", L"PB" };
    unsigned i = 0;

    while (bytes >= 10000) {
        bytes /= 1000;
        i++;
    }
    *units = si[(i >= BUFLEN(si) ? BUFLEN(si) - 1 : i)];
    *signif = bytes;
}


/** @brief Formats time in nanoseconds to the most natural units
 *  @param[in,out] ns
 *      On input, time in seconds. On output, time in whatever units are placed
 *      in @p si
 *  @param[out] si
 *      Pointer to location that the unit string is stored to
 */
static void qagen_copy_format_time(double *sec, const wchar_t **si)
{
    static const wchar_t *pfix[] = { L"ns", L"μs", L"ms", L"s", L"min", L"hr", L"day" };
    unsigned i = 3;

    if (*sec < 1e0) {
        do {
            *sec *= 1e3;
            i--;
        } while (i && *sec < 1e0);
    } else if (*sec > 60.0) {
        *sec /= 60.0;
        i++;
        if (*sec > 60.0) {
            *sec /= 60.0;
            i++;
            if (*sec > 24.0) {
                *sec /= 24.0;
                i++;
            }
        }
    }
    *si = pfix[i];
}


static void qagen_copy_show_time(LARGE_INTEGER t0, LARGE_INTEGER t1)
{
    LARGE_INTEGER freq;
    const wchar_t *si;
    double seconds;

    QueryPerformanceFrequency(&freq);
    seconds = ((double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart);
    qagen_copy_format_time(&seconds, &si);
    qagen_log_printf(QAGEN_LOG_INFO, L"Operation complete! Took %.1f %s", seconds, si);
}


/** @brief Computes the size of the dose beam list
 *  @details If the list contains DICOM files, it sums their individual sizes.
 *      If it contains MHD files, it multiplies the size of the RD template by
 *      the total number of dose beams
 *  @param pt
 *      Patient context
 *  @returns The total file size of the Dose Beam list, or zero on error/empty
 *      list
 */
static ULONGLONG qagen_copy_dosebeam_size(struct qagen_copy_ctx      *ctx,
                                          const struct qagen_patient *pt)
{
    ULONGLONG res = 0;
    uint32_t dblen;

    if (pt->dose_beam) {
        if (pt->dose_beam->type == QAGEN_FILE_DCM_DOSEBEAM) {
            res = qagen_file_list_totalsize(pt->dose_beam);
        } else {
            /* ctx->templatesz = qagen_file_list_totalsize(pt->rd_template);
            dblen = qagen_file_list_len(pt->dose_beam);
            res = ctx->templatesz * dblen; */
            /* Ugh, just take the total size of all RD files I guess */
            res = qagen_file_list_totalsize(pt->rtdose);
            dblen = qagen_file_list_len(pt->rtdose);
            ctx->templatesz = res / dblen;
            qagen_log_printf(QAGEN_LOG_DEBUG, L"Computed template size: %u", ctx->templatesz);
        }
    }
    return res;
}


/** @brief Computes the total number of bytes that must be written to the
 *      destination directory
 *  @param[out] ctx
 *      Copy context
 *  @param pt
 *      Patient context
 *  @returns Nonzero on error (if this function fails, it logs an error and
 *      returns zero)
 */
static int qagen_copy_compute_total(struct qagen_copy_ctx      *ctx,
                                    const struct qagen_patient *pt)
{
    ctx->total += qagen_file_list_totalsize(pt->rtplan);
    ctx->total += qagen_file_list_totalsize(pt->rtdose);
    ctx->total += qagen_copy_dosebeam_size(ctx, pt);
    return 0;
}


/** @brief Computes the number of files that must be written to the destination
 *      directory
 *  @param[out] ctx
 *      Copy context
 *  @param pt
 *      Patient context
 *  @returns Nonzero on error (this function cannot fail)
 */
static int qagen_copy_compute_nfiles(struct qagen_copy_ctx      *ctx,
                                     const struct qagen_patient *pt)
{
    ctx->nfiles += qagen_file_list_len(pt->rtplan);
    ctx->nfiles += qagen_file_list_len(pt->rtdose);
    ctx->nfiles += qagen_file_list_len(pt->dose_beam);
    return 0;
}


/** @brief Writes the title of the progress dialog for this patient
 *  @param[out] ctx
 *      Copy context
 *  @param pt
 *      Patient context
 *  @returns Nonzero on error (this function cannot fail)
 */
static int qagen_copy_write_title(struct qagen_copy_ctx      *ctx,
                                  const struct qagen_patient *pt)
{
    swprintf(ctx->title, BUFLEN(ctx->title), L"Copying %.4s (patient %d of %d)",
             pt->foldername, pt->pt_idx + 1, pt->pt_tot);
    return 0;
}


/** @brief Prepares the copy context for copying the files
 *  @details We need:
 *      - The total number of bytes
 *      - The total number of files
 *      - A title for the progress dialog (it does not change during the copy)
 *  @param ctx
 *      Copy context
 *  @param pt
 *      Patient context
 *  @returns Nonzero on error
 */
static int qagen_copy_prepare(struct qagen_copy_ctx      *ctx,
                              const struct qagen_patient *pt)
{
    const wchar_t *si;
    int signif;

    if (qagen_copy_compute_total(ctx, pt)
     || qagen_copy_compute_nfiles(ctx, pt)
     || qagen_copy_write_title(ctx, pt)) {
        return 1;
    }
    qagen_copy_format_bytes(ctx->total, &si, &signif);
    qagen_log_printf(QAGEN_LOG_INFO, L"Ready to copy: %u file%s totaling %d %s", ctx->nfiles, PLFW(ctx->nfiles), signif, si);
    return 0;
}


/** @brief Wrapper for CopyFileEx
 *  @param ctx
 *      Copy context
 *  @param exist
 *      Path to existing file
 *  @param dest
 *      Path to desination file
 *  @returns Nonzero if the op should be cancelled, which includes error
 *      states. Check the error state to distinguish a cancel from an error
 */
static int qagen_copy_wrap(struct qagen_copy_ctx *ctx,
                           const wchar_t         *exist,
                           const wchar_t         *dest)
{
    static const wchar_t *failmsg = L"Failed to copy file";
    DWORD lasterr;

    ctx->curcopy = 0;
    if (!CopyFileEx(exist, dest, qagen_copy_proc, ctx, &ctx->opcancel, 0)) {
        lasterr = GetLastError();
        if (lasterr != ERROR_REQUEST_ABORTED) {
            qagen_error_raise(QAGEN_ERR_WIN32, &lasterr, failmsg);
        }
        return 1;
    } else {
        ctx->ncopied++;
        return 0;
    }
}


/** @brief Sets line 1 to context-specific information
 *  @param ctx
 *      Copy context
 *  @param ismhd
 *      true if this is an MHD file
 */
static void qagen_copy_set_message(struct qagen_copy_ctx *ctx,
                                   bool                   ismhd)
{
    const wchar_t *si, *op = (ismhd) ? L"Converting" : L"Copying";
    int signif;

    qagen_copy_format_bytes(ctx->total - ctx->completed, &si, &signif);
    swprintf(ctx->line1, BUFLEN(ctx->line1),
             L"%s file %d of %d (%d %s remaining)",
             op, ctx->ncopied + 1, ctx->nfiles, signif, si);
}


/** @brief Sets line 2 to Name: <filename>
 *  @param ctx
 *      Copy context
 *  @param name
 *      Filename
 */
static void qagen_copy_set_filename(struct qagen_copy_ctx *ctx,
                                    const wchar_t         *name)
{
    swprintf(ctx->line2, BUFLEN(ctx->line2), L"Name: %s", name);
}


/** @brief Copies the single RTPlan contained by @p pt
 *  @param ctx
 *      Copy context
 *  @param pt
 *      Patient context
 *  @returns Nonzero on error
 */
static int qagen_copy_rtplan(struct qagen_copy_ctx *ctx,
                             struct qagen_patient  *pt)
{
    int res;

    if (qagen_path_join(&pt->basepath, pt->rtplan->name)) {
        return 1;
    }
    qagen_copy_set_filename(ctx, pt->rtplan->name);
    res = qagen_copy_wrap(ctx, pt->rtplan->path, pt->basepath->buf);
    qagen_path_remove_filespec(&pt->basepath);
    return res;
}


/** @brief Copies all RTDose files contained by @p pt
 *  @param ctx
 *      Copy context
 *  @param pt
 *      Patient context
 *  @returns Nonzero on error
 */
static int qagen_copy_rtdose(struct qagen_copy_ctx *ctx,
                             struct qagen_patient  *pt)
{
    wchar_t rename[MAX_PATH];   /* Humongous fixed-size buffer? Yes please */
    const struct qagen_file *rd = pt->rtdose;
    int res = 0;

    for (; rd && !res; rd = rd->next) {
        swprintf(rename, BUFLEN(rename), L"%d-%s", rd->data.rd.beamnum, rd->name);
        if (qagen_path_join(&pt->basepath, rename)) {
            return 1;
        }
        qagen_copy_set_filename(ctx, rd->name);
        res = qagen_copy_wrap(ctx, rd->path, pt->basepath->buf);
        qagen_path_remove_filespec(&pt->basepath);
    }
    return res;
}


static void qagen_copy_mhd_prepare(struct qagen_copy_ctx   *ctx,
                                   const struct qagen_file *mhd)
{
    qagen_copy_set_message(ctx, true);
    qagen_copy_set_filename(ctx, mhd->name);
    qagen_copy_update_dlg(ctx);
}


static void qagen_copy_mhd_complete(struct qagen_copy_ctx *ctx)
{
    ctx->completed += ctx->templatesz;
    ctx->ncopied++;
    qagen_copy_update_dlg(ctx);
}


/** @brief Converts the MHD/RAW files into DICOM files in the destination
 *      directory
 *  @param ctx
 *      Copy context
 *  @param pt
 *      Patient context
 *  @returns Nonzero on error
 *  @note This function must update the progress dialog itself, because no
 *      calls are made to the copy progress routine
 */
static int qagen_copy_mhd_dosebeams(struct qagen_copy_ctx *ctx,
                                    struct qagen_patient  *pt)
{
    const struct qagen_file *mhd = pt->dose_beam;
    const wchar_t *const template = (pt->rd_template) ? pt->rd_template->path : pt->rtdose->path;
    int res = 0;

    for (; mhd && !res && !qagen_progdlg_cancelled(&ctx->pdlg); mhd = mhd->next) {
        if (qagen_path_join(&pt->basepath, mhd->name)) {
            return 1;
        }
        qagen_copy_mhd_prepare(ctx, mhd);
        res = qagen_path_rename_extension(&pt->basepath, L"dcm")
           || qagen_metaio_convert(mhd->path, pt->basepath->buf, template);
        qagen_path_remove_filespec(&pt->basepath);
        if (!res) {
            qagen_copy_mhd_complete(ctx);
        }
    }
    return res;
}


/** @brief Copies the Dose_Beams, under the assumption that they are DICOM
 *      files
 *  @param ctx
 *      Copy context
 *  @param pt
 *      Patient context
 *  @returns Nonzero on error
 */
static int qagen_copy_dcm_dosebeams(struct qagen_copy_ctx *ctx,
                                    struct qagen_patient  *pt)
{
    const struct qagen_file *db = pt->dose_beam;
    int res = 0;

    for (; db && !res; db = db->next) {
        if (qagen_path_join(&pt->basepath, db->name)) {
            return 1;
        }
        qagen_copy_set_filename(ctx, db->name);
        res = qagen_copy_wrap(ctx, db->path, pt->basepath->buf);
        qagen_path_remove_filespec(&pt->basepath);
    }
    return res;
}


/** @brief Copies all Dose_Beams contained by @p pt
 *  @param ctx
 *      Copy context
 *  @param pt
 *      Patient context
 *  @returns Nonzero on error
 */
static int qagen_copy_dosebeams(struct qagen_copy_ctx *ctx,
                                struct qagen_patient  *pt)
{
    if (!pt->dose_beam) {
        return 0;
    }
    if (pt->dose_beam->type == QAGEN_FILE_MHD_DOSEBEAM) {
        return qagen_copy_mhd_dosebeams(ctx, pt);
    } else {
        return qagen_copy_dcm_dosebeams(ctx, pt);
    }
}


/** @brief Copies all relevant files to the patient directory
 *  @param ctx
 *      Copy context
 *  @param pt
 *      Patient context
 *  @returns Nonzero on error
 *  @note The patient directory must exist by the time this function is called
 */
static int qagen_copy_files(struct qagen_copy_ctx *ctx,
                            struct qagen_patient  *pt)
{
    LARGE_INTEGER t0, t1;
    int res;

    if (qagen_progdlg_show(&ctx->pdlg, ctx->title)) {
        return 1;
    }
    QueryPerformanceCounter(&t0); /* This can't fail on XP or later.  */
    res = qagen_copy_rtplan(ctx, pt)
       || qagen_copy_rtdose(ctx, pt)
       || qagen_copy_dosebeams(ctx, pt);
    QueryPerformanceCounter(&t1);
    qagen_progdlg_destroy(&ctx->pdlg);
    if (!res) {
        qagen_copy_show_time(t0, t1);
    }
    return res;
}


int qagen_copy_patient(struct qagen_patient *pt)
{
    struct qagen_copy_ctx ctx = { 0 };

    return qagen_copy_prepare(&ctx, pt)
        || qagen_copy_files(&ctx, pt);
}


int qagen_copy_update_dlg(struct qagen_copy_ctx *ctx)
{
    return qagen_progdlg_set_progress(&ctx->pdlg, ctx->completed, ctx->total)
        || qagen_progdlg_set_line(&ctx->pdlg, ctx->line1, 1)
        || qagen_progdlg_set_line(&ctx->pdlg, ctx->line2, 2);
}


static void qagen_copy_update(struct qagen_copy_ctx *ctx)
{
    wchar_t *si;
    int signif;

    qagen_copy_format_bytes(ctx->total - ctx->completed, &si, &signif); /* Wait wtf I'm not even using this! */
    qagen_copy_set_message(ctx, false);
    if (qagen_copy_update_dlg(ctx)) {
        ctx->hide = TRUE;
    }
}


DWORD qagen_copy_proc(LARGE_INTEGER totalsz, LARGE_INTEGER totalxfer,
                      LARGE_INTEGER strmsz,  LARGE_INTEGER strmxfer,
                      DWORD         strmno,  DWORD         cbreason,
                      HANDLE        hsrc,    HANDLE        hdest,
                      struct qagen_copy_ctx *cctx)
{
    LONGLONG diff = totalxfer.QuadPart - cctx->curcopy;

    cctx->completed += diff;
    cctx->curcopy = totalxfer.QuadPart;
    qagen_copy_update(cctx);
    if (cctx->opcancel || qagen_progdlg_cancelled(&cctx->pdlg)) {
        qagen_log_puts(QAGEN_LOG_INFO, L"Cancelling transfer");
        cctx->opcancel = TRUE;
        return PROGRESS_CANCEL;
    } else if (cctx->hide) {
        /* Probably destroy the window here
        I will have to be sure that the cleanup routines are still safe */
        return PROGRESS_QUIET;
    } else {
        return PROGRESS_CONTINUE;
    }
}
