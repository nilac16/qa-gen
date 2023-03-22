#pragma once
/** @file This file manages the thread-local error state
 * 
 *  @note The error state is used to distinguish between cancelling an
 *      operation normally and due to error. If an error occurs, but you don't
 *      want to cancel the algorithm, *be sure to clear it before progressing*
 */
#ifndef QAGEN_ERROR_H
#define QAGEN_ERROR_H

#include "qagen-defs.h"

EXTERN_C_START


enum {
    QAGEN_ERR_NONE,
    QAGEN_ERR_WIN32,
    QAGEN_ERR_HRESULT,
    QAGEN_ERR_SYSTEM,
    QAGEN_ERR_RUNTIME
};


struct qagen_error {
    int type;   /* One of the above enumerations */
    
    wchar_t context[128];   /* What we were doing when the error occurred */
    wchar_t message[128];   /* The library's message about the error */
    /* wchar_t extra[128]; *//* More helpful information, maybe (consider it) */

    HRESULT hr;
    DWORD   dwerr;
    int     errnum;
};


/** @brief Raises an error state
 *  @details The error state is maintained as a thread_local "global" in the
 *      file scope of the implementation file, similar to errno in stdlib. If
 *      an error state is raised in a thread, it can only be removed by a call
 *      to this function with QAGEN_ERR_NONE
 *  @param type
 *      The error type, given as an enumeration value from QAGEN_ERR_* above
 *  @param data
 *      Data necessary for the error. This is @b required to be a pointer to the
 *      offending HRESULT for HRESULT errors. In every other case, it is
 *      optional, and will be fetched by the routine if NULL, either from errno
 *      or GetLastError(), so be sure that value is still correct if you have
 *      not saved it. Runtime errors expect this to be a pointer to the context
 *      string
 *  @param fmt
 *      Format string containing context information
 *  @note For runtime errors, the format string is used as the MESSAGE
 *      string, and the string contained by @p data is placed into the context
 *      buffer. Adjust your usage for sensible error reporting
 */
void qagen_error_raise(int type, const void *data, const wchar_t *restrict fmt, ...);


/** @todo Consider whether a function like this would be useful (yes probably).
 *      The tradeoff would be extra complexity in certain functions (but not as
 *      bad as try-catch spam in C++)
 *  @brief Changes the context string of the currently raised error
 *  @param fmt
 *      Format string specifying the new context
 */
void qagen_error_set_context(const wchar_t *restrict fmt, ...);


/** @brief Places the string pointers in its args
 *  @param ctx
 *      Location where the context string shall be written
 *  @param msg
 *      Location where the library error message shall be written
 */
void qagen_error_string(const wchar_t **ctx, const wchar_t **msg);


/** @brief Returns true if this thread's error indicator is set
 *  @returns true if the error state is not QAGEN_ERROR_NONE, false otherwise
 *  @warning The error state does not keep itself internally consistent. If an
 *      error is raised, and you want to ignore it, *CLEAR IT*! If you do not,
 *      this call becomes useless
 */
bool qagen_error_state(void);


EXTERN_C_END

#endif /* QAGEN_ERROR_H */
