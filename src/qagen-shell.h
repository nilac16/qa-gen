#pragma once
/** @file Most of the shell operations should be done here, like searching
 *      directories and copying files
 */
#ifndef QAGEN_SHELL_H
#define QAGEN_SHELL_H

#include "qagen-defs.h"


/** Internal note: Distinguish between four states, despite the cardinality of
 *  the result enum
 *   - Closed: User closed: Exit the application without prompt
 *   - Normal: Operation completed normally, ask to continue
 *   - Error:  Display an error message, ask to continue
 *  But also
 *   - Cancel: User cancelled the operation somehow, ask to continue
 * 
 *  If the user cancels the operation, it will be indistinguishable from normal
 *  state to the enclosing scope, but it will have to be propagated within this
 *  file
 */


typedef enum qagen_shl_res {
    SHELL_CLOSED = -1,  /* The user closed the window, so don't ask to continue */
    SHELL_NORMAL =  0,  /* Algorithm completed normally, ask to continue */
    SHELL_ERROR  =  1   /* Error occurred, display and ask to continue */
} qagen_shlres_t;


/** @brief Runs the part of the application that actually does things
 *  @returns One of the qagen_shlres_t enum constants
 */
qagen_shlres_t qagen_shell_run(void);


#endif /* QAGEN_SHELL_H */
