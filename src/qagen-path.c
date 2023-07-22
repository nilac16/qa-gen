#include <stdio.h>
#include "qagen-path.h"
#include "qagen-memory.h"
#include "qagen-error.h"
#include "qagen-log.h"
#include <PathCch.h>


PATH *qagen_path_create(const wchar_t *path)
{
    size_t len;
    PATH *res;

    len = wcslen(path);
    res = qagen_malloc(sizeof *res + sizeof *res->buf * (len + 1));
    if (res) {
        res->pathlen = len;
        res->buflen = len + 1;
        wcscpy(res->buf, path);
    }
    return res;
}


PATH *qagen_path_duplicate(const PATH *path)
{
    const size_t blksz = sizeof *path + sizeof *path->buf * path->buflen;
    PATH *res;

    res = qagen_malloc(blksz);
    if (res) {
        memcpy(res, path, blksz);
    }
    return res;
}


void qagen_path_free(PATH *path)
{
    qagen_free(path);
}


/** @brief Reallocates @p path if @p buflen is greater than its current buffer
 *      size
 *  @param path
 *      Pointer to path pointer
 *  @param buflen
 *      Required buffer size
 *  @returns Nonzero on error. On error, @p path is unchanged
 *  @note Yes I know this is strict and not monotonic, monotonic just kind of
 *      sounds cool ok
 */
static int qagen_path_monotonic(PATH **path, size_t buflen)
{
    size_t size;
    void *newptr;

    if (buflen > (*path)->buflen) {
        size = sizeof **path + sizeof *(*path)->buf * buflen;
        newptr = qagen_realloc(*path, size);
        if (newptr) {
            *path = newptr;
            (*path)->buflen = buflen;
        } else {
            return 1;
        }
    }
    return 0;
}


int qagen_path_join(PATH **root, const wchar_t *ext)
/** AFAIK, nothing can expand to a string larger than root + ext + 1
 */
{
    static const wchar_t *failmsg = L"Failed to combine path strings";
    size_t reqlen;
    HRESULT hr;

    reqlen = (*root)->pathlen + wcslen(ext) + 2;
    if (!qagen_path_monotonic(root, reqlen)) {
        hr = PathCchCombineEx((*root)->buf, (*root)->buflen, (*root)->buf, ext, PATHCCH_ALLOW_LONG_PATHS);
        if (SUCCEEDED(hr)) {
            (*root)->pathlen = wcslen((*root)->buf);
            return 0;
        } else {
            qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failmsg);
        }
    }
    return 1;
}


void qagen_path_remove_filespec(PATH **path)
{
    /* HRESULT hr; */

    PathCchRemoveFileSpec((*path)->buf, (*path)->buflen);
    (*path)->pathlen = wcslen((*path)->buf);
}


void qagen_path_remove_extension(PATH **path)
{
    HRESULT hr;

    do {
        hr = PathCchRemoveExtension((*path)->buf, (*path)->buflen);
    } while (hr == S_OK);
    (*path)->pathlen = wcslen((*path)->buf);
}


int qagen_path_rename_extension(PATH **path, const wchar_t *ext)
{
    static const wchar_t *failmsg = L"Failed to rename file extension";
    size_t reqlen;
    HRESULT hr;

    reqlen = (*path)->pathlen + wcslen(ext) + 2;
    if (!qagen_path_monotonic(path, reqlen)) {
        hr = PathCchRenameExtension((*path)->buf, (*path)->buflen, ext);
        if (SUCCEEDED(hr)) {
            (*path)->pathlen = wcslen((*path)->buf);
            return 0;
        } else {
            qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failmsg);
        }
    }
    return 1;
}


bool qagen_path_is_subdirectory(const WIN32_FIND_DATA *fdata)
{
    if (fdata->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        switch (fdata->cFileName[0]) {
        case L'\0':
            return false;
        case L'.':
            switch (fdata->cFileName[1]) {
            case L'\0':
                return false;
            case L'.':
                return fdata->cFileName[2] != L'\0';
            default:
                return true;
            }
        default:
            return true;
        }
    }
    return false;
}


PATH *qagen_path_to_executable(void)
{
    DWORD len = 64, nwrit;
    PATH *res = NULL;
    void *newptr;

    do {
        len *= 2;
        newptr = qagen_realloc(res, sizeof *res + sizeof *res->buf * len);
        if (!newptr) {
            qagen_ptr_nullify(&res, qagen_path_free);
            break;
        } else {
            res = newptr;
        }
        nwrit = GetModuleFileName(NULL, res->buf, len);
    } while (nwrit == len);
    res->pathlen = nwrit;
    res->buflen = len;
    return res;
}


bool qagen_path_char_isvalid(wchar_t chr)
{
    static const wchar_t invalid[] = {
        L'\"', L'*', L'/', L':', L'<', L'>', L'?', L'\\', L'|'
    };
    unsigned i;

    if (chr < 32) {
        return false;
    }
    for (i = 0; i < BUFLEN(invalid); i++) {
        if (invalid[i] == chr) {
            return false;
        }
    }
    return true;
}
