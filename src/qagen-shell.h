#pragma once
/** @file Most of the shell operations should be done here, like searching
 *      directories and copying files
 */
#ifndef QAGEN_SHELL_H
#define QAGEN_SHELL_H

#include "qagen-defs.h"


/** Use SHELL_NORMAL if the user cancels the transfer
 *  This may be done if the wrong patient is selected by accident
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
