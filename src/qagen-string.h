#pragma once
/** @file Contains common string operations
 */
#ifndef QAGEN_STRING_H
#define QAGEN_STRING_H

#include "qagen-defs.h"


/** @brief Creates a formatted string on the heap
 *  @param fmt
 *      Format string
 *  @returns A pointer to a heap-allocated string formatted according to
 *      @p fmt, or NULL on error. This string must be freed with qagen_free
 *      when you are done with it. The buffer that contains this string is
 *      guaranteed to be exactly large enough to hold it (i.e. its size is
 *      equal to wcslen(str) + 1)
 *  @todo See if Microsoft has a proper wide version of snprintf(3). swprintf(3)
 *      does not share its size-calculating behavior when passed a NULL pointer
 */
wchar_t *qagen_string_createf(const wchar_t *restrict fmt, ...);


__declspec(deprecated) /* This is only used by the obsolete RP window */
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
 *  @note This is a very inefficient function, and you should avoid it wherever
 *      possible
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


/** @brief Determines if @p s is either empty or contains only whitespace
 *  @param s
 *      String
 *  @returns true if @p s is only whitespace or empty, false otherwise
 */
bool qagen_string_isempty(const wchar_t *s);


#endif /* QAGEN_STRING_H */
