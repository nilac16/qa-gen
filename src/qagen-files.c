#include <stdio.h>
#include "qagen-files.h"
#include "qagen-dicom.h"
#include "qagen-error.h"
#include "qagen-string.h"
#include "qagen-log.h"
#include "qagen-memory.h"


static int qagen_file_initialize_data(struct qagen_file *file)
{
    switch (file->type) {
    case QAGEN_FILE_DCM_RP:
        return qagen_rtplan_load(&file->data.rp, file->path);
    case QAGEN_FILE_DCM_RD:
    case QAGEN_FILE_DCM_DOSEBEAM:
        return qagen_rtdose_load(&file->data.rd, file->path);
    case QAGEN_FILE_MHD_DOSEBEAM:
        return 0;
    }
    /* C4715: After case labels for all members of a typed enum? */
    qagen_log_printf(QAGEN_LOG_ERROR, L"Unknown file list type %d", (int)file->type);
    qagen_error_raise(QAGEN_ERR_RUNTIME, L"Bad file list type", L"Type number %d is invalid", (int)file->type);
    return 1;
}


static struct qagen_file *qagen_file_create_node(qagen_file_t   type,
                                                 PATH         **base,
                                                 const wchar_t *name)
{
    size_t size;
    struct qagen_file *node = NULL;
    if (!qagen_path_join(base, name)) {
        size = sizeof *node + sizeof *node->path * ((*base)->pathlen + 1);
        node = qagen_calloc(1UL, size);
        if (node) {
            node->type = type;
            swprintf(node->name, BUFLEN(node->name), L"%s", name);
            wcscpy(node->path, (*base)->buf);
            if (qagen_file_initialize_data(node)) {
                qagen_ptr_nullify(&node, qagen_freezero);
            }
        }
        qagen_path_remove_filespec(base);
    }
    return node;
}


static PATH *qagen_file_make_wildcard(const PATH *dir, const wchar_t *pattern)
{
    PATH *res = qagen_path_duplicate(dir);
    if (res) {
        if (qagen_path_join(&res, pattern)) {
            qagen_ptr_nullify(&res, qagen_path_free);
        }
    }
    return res;
}


/** If the pattern is NULL, this returns an invalid handle without raising an
 *  error
 */
static HANDLE qagen_file_find_first(const PATH      *pattern,
                                    WIN32_FIND_DATA *fdata)
{
    static const wchar_t *failmsg = L"Failed to open file FindFirst handle";
    HANDLE res = INVALID_HANDLE_VALUE;
    if (pattern) {
        res = FindFirstFile(pattern->buf, fdata);
        if (res == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            switch (err) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
                break;
            default:
                qagen_error_raise(QAGEN_ERR_WIN32, &err, failmsg);
                break;
            }
        }
    }
    return res;
}


static bool qagen_file_find_next(HANDLE hfile, WIN32_FIND_DATA *fdata)
{
    static const wchar_t *failmsg = L"Failed to find next file from FindFirst handle";
    bool res = FindNextFile(hfile, fdata);
    if (!res) {
        DWORD err = GetLastError();
        switch (err) {
        case ERROR_NO_MORE_FILES:
            break;
        default:
            qagen_error_raise(QAGEN_ERR_WIN32, &err, failmsg);
            break;
        }
    }
    return res;
}


struct qagen_file *qagen_file_enumerate(qagen_file_t   type,
                                        const PATH    *dir,
                                        const wchar_t *pattern)
{
    static const wchar_t *failmsg = L"Failed to enumerate files";
    struct qagen_file *res = NULL, **end = &res;
    WIN32_FIND_DATA fdata;
    PATH *wildcard = qagen_file_make_wildcard(dir, pattern);
    HANDLE hfile = qagen_file_find_first(wildcard, &fdata);
    if (hfile != INVALID_HANDLE_VALUE) {
        qagen_path_remove_filespec(&wildcard);
        do {
            *end = qagen_file_create_node(type, &wildcard, fdata.cFileName);
            if (*end) {
                end = &(*end)->next;
            } else {
                qagen_ptr_nullify(&res, qagen_file_list_free);
                break;
            }
        } while (qagen_file_find_next(hfile, &fdata));
        FindClose(hfile);
    }
    qagen_path_free(wildcard);
    return res;
}


/** Not NULL-tolerant */
static void qagen_file_free_node(struct qagen_file *node)
{
    switch (node->type) {
    case QAGEN_FILE_DCM_RP:
        qagen_rtplan_destroy(&node->data.rp);
        break;
    case QAGEN_FILE_DCM_RD:
    case QAGEN_FILE_DCM_DOSEBEAM:
        qagen_rtdose_destroy(&node->data.rd);
        break;
    }
    qagen_freezero(node);
}


static bool qagen_file_filterable(const struct qagen_file *rd,
                                  const struct qagen_file *rp)
{
    return !qagen_rtdose_instance_match(&rd->data.rd, &rp->data.rp)
        || !qagen_rtdose_isnumbered(&rd->data.rd);
}


void qagen_file_filter_rd(struct qagen_file **rd, const struct qagen_file *rp)
{
    struct qagen_file *del;
    qagen_log_printf(QAGEN_LOG_DEBUG, L"Filtering RP SOPInstanceUID %S",
        rp->data.rp.sop_inst_uid);
    while (*rd) {
        if (qagen_file_filterable(*rd, rp)) {
            del = *rd;
            *rd = (*rd)->next;
            qagen_file_free_node(del);
        } else {
            rd = &(*rd)->next;
        }
    }
}


void qagen_file_list_free(struct qagen_file *head)
{
    struct qagen_file *next;
    for (; head; head = next) {
        next = head->next;
        qagen_file_free_node(head);
    }
}


uint32_t qagen_file_list_len(const struct qagen_file *head)
{
    uint32_t res = 0;
    for (; head; head = head->next) {
        res++;
    }
    return res;
}


struct qagen_file *qagen_file_list_extract(struct qagen_file *head, int idx)
{
    struct qagen_file *res, **eptr = &head;
    while (idx--) {
        eptr = &(*eptr)->next;
    }
    res = *eptr;
    *eptr = (*eptr)->next;
    qagen_file_list_free(head);
    res->next = NULL;
    return res;
}


/** @note This function is pretty bad... Please rewrite this to be a little bit
 *      more sane
 *  @note LOLOLOLOLOLOLOL computers are fast lmao
 */
static wchar_t *qagen_file_single_string(const struct qagen_file *head)
{
    wchar_t *res = qagen_string_createf(L"{(%s: %.2f MU)",
                                        head->data.rp.beam[0].name,
                                        head->data.rp.beam[0].meterset);
    size_t count;
    if (res) {
        count = wcslen(res) + 1;
        for (size_t i = 1; i < head->data.rp.nbeams; i++) {
            if (qagen_string_concatf(&res, &count, L", (%s: %.2f MU)",
                                     head->data.rp.beam[i].name,
                                     head->data.rp.beam[i].meterset)) {
                qagen_free(res);
                return NULL;
            }
        }
        if (qagen_string_concatf(&res, &count, L"}")) {
            qagen_ptr_nullify(&res, qagen_free);
        }
    }
    return res;
}


static int qagen_file_write_strings(const struct qagen_file *head,
                                    wchar_t                **str,
                                    const int                count)
{
    for (int i = 0; i < count; i++) {
        str[i] = qagen_file_single_string(head);
        if (!str[i]) {
            qagen_file_beam_strings_free(count, str);
        }
    }
    return count;
}


int qagen_file_beam_strings(const struct qagen_file *head, wchar_t ***str)
{
    int count = 0;
    if (head->type == QAGEN_FILE_DCM_RP) {
        count = qagen_file_list_len(head);
        *str = qagen_calloc(count, sizeof **str);
        if (*str) {
            count = qagen_file_write_strings(head, *str, count);
        } else {
            count = 0;
        }
    } else {
        qagen_error_raise(QAGEN_ERR_RUNTIME, L"Cannot get beam information", L"List does not contain DICOM RTPlans");
    }
    return count;
}


void qagen_file_beam_strings_free(int nstr, wchar_t **str)
{
    if (str) {
        for (int i = 0; i < nstr; i++) {
            qagen_free(str[i]);
        }
        qagen_free(str);
    }
}


/** @brief Gets the size of the file contained by @p file
 *  @param file
 *      File list node
 *  @returns The file size, or zero on error
 */
static ULONGLONG qagen_file_size(const struct qagen_file *file)
{
    static const wchar_t *failmsg = L"Failed to get the size of a file";
    const DWORD access = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    LARGE_INTEGER sz = { .QuadPart = 0 };
    HANDLE hfile = CreateFile(file->path,
                              0,
                              access,
                              NULL,
                              OPEN_EXISTING,
                              0,
                              NULL);
    if (hfile != INVALID_HANDLE_VALUE) {
        if (!GetFileSizeEx(hfile, &sz)) {
            qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
            sz.QuadPart = 0;
        }
        CloseHandle(hfile);
    } else {
        qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
    }
    return sz.QuadPart;
}


ULONGLONG qagen_file_list_totalsize(const struct qagen_file *file)
{
    ULONGLONG res = 0, inc;
    for (; file; file = file->next) {
        inc = qagen_file_size(file);
        if (inc) {
            res += qagen_file_size(file);
        } else {
            qagen_log_printf(QAGEN_LOG_ERROR, L"%s: File size is zero", file->name);
        }
    }
    return res;
}
