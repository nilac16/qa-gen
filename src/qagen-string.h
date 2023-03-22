#pragma once
/** @file Contains common string operations
 */
#ifndef QAGEN_STRING_H
#define QAGEN_STRING_H

#include "qagen-defs.h"


/* Consider it. It may be the solution to the concatf problem
typedef struct {
    size_t  strlen;
    size_t  buflen;
    wchar_t buf[];
} STRING; */


/** @brief Creates a formatted string on the heap
 *  @param fmt
 *      Format string
 *  @returns A pointer to a heap-allocated string formatted according to
 *      @p fmt, or NULL on error. This string must be freed with qagen_free
 *      when you are done with it. The buffer that contains this string is
 *      guaranteed to be exactly large enough to hold it (i.e. its size is
 *      equal to wcslen(str) + 1)
 *  @note This operation is not efficient with wchar_t strings
 */
wchar_t *qagen_string_createf(const wchar_t *restrict fmt, ...);


/** @todo A drastic rewrite of this algorithm
 *  @brief Concatenates formatted output to @p dst
 *  @param dst
 *      Destination string. This must point to a valid non-const string
 *  @param dstcount
 *      Buffer *count* of @p dst! This is not the string length, nor is it the
 *      buffer size in bytes! On exit, this is the new buffer size, which may
 *      be substantially larger than the length of the contained string
 *  @param fmt
 *      Format string to be concatenated
 *  @returns Nonzero on error. On error, the contents of @p dst are undefined
 *  @note This is a rather complicated operation, and should be avoided if
 *      an evidently efficient solution exists
 */
int qagen_string_concatf(wchar_t       *restrict *dst,
                         size_t                  *dstcount,
                         const wchar_t *restrict  fmt,
                         ...);


/** @brief Mallocs a UTF-16 copy of input string @p utf8
 *  @param utf8
 *      UTF-8 input string to be copied
 *  @returns A heap-allocated UTF-16 copy of @p utf8
 *  @note This just jumps to qagen_string_createf with L"%S" as its format
 *      string
 */
wchar_t *qagen_string_utf16cvt(const char *utf8);


#endif /* QAGEN_STRING_H */
