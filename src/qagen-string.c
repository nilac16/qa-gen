#include <stdio.h>
#include "qagen-string.h"
#include "qagen-memory.h"
#include "qagen-error.h"
#include "qagen-log.h"

#define CREATEF_INITIAL_LEN 64


/** @brief Computes the buffer size needed for a string formatted according to
 *      @p fmt and @p args
 *  @param fmt
 *      Format string specifying the output's contents
 *  @param args
 *      Format string arguments
 *  @returns The *buffer length* needed for the output string, or zero on error
 */
static size_t qagen_string_get_fmtsize(const wchar_t *restrict fmt, va_list args)
{
    static const wchar_t *failmsg = L"Failed to compute size of formatted output";
    int code = _vsnwprintf(NULL, 0, fmt, args);
    if (code < 0) {
        qagen_error_raise(QAGEN_ERR_SYSTEM, &(const int){ EILSEQ }, failmsg);
        return 0;
    } else {
        return (size_t)code + 1;
    }
}


wchar_t *qagen_string_createf(const wchar_t *restrict fmt, ...)
{
    wchar_t *res = NULL;
    va_list args;
    size_t len;
    va_start(args, fmt);
    len = qagen_string_get_fmtsize(fmt, args);
    va_end(args);
    if (len) {
        res = qagen_malloc(sizeof *res * len);
        if (res) {
            va_start(args, fmt);
            _vsnwprintf(res, len, fmt, args);
            va_end(args);
        }
    }
    return res;
}


int qagen_string_concatf(wchar_t       *restrict *dst,
                         size_t                  *dstcount,
                         const wchar_t *restrict  fmt,
                         ...)
{
    static const wchar_t *failmsg = L"Failed to concatenate formatted string";
    const size_t origlen = wcslen(*dst);
    size_t extra, needed;
    va_list args;
    va_start(args, fmt);
    extra = qagen_string_get_fmtsize(fmt, args);
    va_end(args);
    if (extra) {
        needed = origlen + extra;
        if (needed > *dstcount) {
            wchar_t *test = qagen_realloc(*dst, sizeof *test * needed);
            if (test) {
                *dst = test;
                *dstcount = needed;
            } else {
                return 1;
            }
        }
        va_start(args, fmt);
        vswprintf(*dst + origlen, extra, fmt, args);
        va_end(args);
        return 0;
    } else {
        return 1;
    }
}


wchar_t *qagen_string_utf16cvt(const char *utf8)
{
    return qagen_string_createf(L"%S", utf8);
}


bool qagen_string_isempty(const wchar_t *s)
{
    bool res = true;
    while (*s && res) {
        res = iswspace(*s);
        s++;
    }
    return res;
}
