#pragma once
/** @file Logging functions, very generic
 */
#ifndef QAGEN_LOG_H
#define QAGEN_LOG_H

#include "qagen-defs.h"

EXTERN_C_START


typedef enum qagen_log_lvl {
    QAGEN_LOG_DEBUG,
    QAGEN_LOG_INFO,
    QAGEN_LOG_WARN,
    QAGEN_LOG_ERROR
} qagen_loglvl_t;


/** Log callback: Message first, user data, then log level last.
 *  The log file structure does thresholding for you, so there is no need to
 *  do it yourself in the callback
 */
typedef int (*qagen_logfn_t)(const wchar_t *, void *, qagen_loglvl_t);


/** Log context, contains a threshold level and user data along with a callback
 *  function
 */
struct qagen_log {
    qagen_loglvl_t threshold;
    qagen_logfn_t  callback;
    void          *cbdata;
};


/** @brief Emplaces the log structure @p log at the head of the log list
 *  @param log
 *      Log file context structure to be added. The caller is resposible for
 *      its memory
 *  @returns Nonzero on error
 */
int qagen_log_add(struct qagen_log *log);


/** @brief Deletes all logging callbacks */
void qagen_log_cleanup(void);


/** @brief Sends @p s to all logging callbacks
 *  @param lvl
 *      Logging level. If a callback's threshold is greater than this, it is
 *      skipped
 *  @param s
 *      String to send
 */
void qagen_log_puts(qagen_loglvl_t lvl, const wchar_t *s);


/** @brief Sends formatted output to all logging callbacks
 *  @param lvl
 *      Logging level
 *  @param fmt
 *      Format string
 */
void qagen_log_printf(qagen_loglvl_t lvl, const wchar_t *restrict fmt, ...);


EXTERN_C_END

#endif /* QAGEN_LOG_H */
