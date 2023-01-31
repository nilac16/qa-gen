#pragma once
/** @file A console attached to this process, used for posting log messages to
 *      the user
 */
#ifndef QAGEN_CONSOLE_H
#define QAGEN_CONSOLE_H

#include "qagen-defs.h"
#include "qagen-log.h"


struct qagen_console {
    HANDLE hcons;
};


/** @brief Allocates a console and fetches its handle
 *  @param cons
 *      Console object to initialize
 *  @returns Nonzero on error
 */
int qagen_console_init(struct qagen_console *cons);


/** @brief Detaches @p cons from this process
 *  @param cons
 *      Console object to destroy
 */
void qagen_console_destroy(struct qagen_console *cons);


/** @brief Writes prefixed, colored output to the console handle. See
 *      qagen_logfn_t for details
 */
int qagen_console_callback(const wchar_t *restrict msg,
                           struct qagen_console   *cons,
                           qagen_loglvl_t          lvl);


#endif /* QAGEN_CONSOLE_H */
