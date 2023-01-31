#pragma once
/** @file The application state
 * 
 *  Welcome to hecc
 * 
 *  This is a rewrite of the classic application, meant to be more abstract and
 *  easier to change later
 * 
 *  This of course means that error messages presented to the user will be nigh
 *  useless, since they would have been posted context-free by the API call
 *  that caused them
 * 
 *  Also I have not simulated *any* failing exit paths, so disaster may strike
 *  at any moment
 */
#ifndef QAGEN_APP_H
#define QAGEN_APP_H

#include "qagen-defs.h"
#include "qagen-rpwnd.h"
#include "qagen-console.h"


/** New rule: Do not memset anything to zero inside a C constructor (C++
 *  constructors may still zero their contents). Always empty-initialize your
 *  structs in their owning scopes!
 */


struct qagen_app {
    HINSTANCE hinst;
    wchar_t  *cmdline;
    int       ncmdshow;

    struct qagen_console cons;
};


/** @brief Initializes the application. The very first thing this function
 *  does is to set the static application state pointer
 *  @param app
 *      Application state object. The first three members must be set
 *  @returns Nonzero on error
 */
int qagen_app_open(struct qagen_app *app);


/** @brief Runs the application
 *  @returns Nonzero on error
 */
int qagen_app_run(void);


/** @brief Cleans up all resources */
void qagen_app_close(void);


/** @brief Fetches the static application state pointer
 *  @returns The application state pointer. Do note that this is only nonnull
 *      while the application is running (i.e. after a call to qagen_app_open,
 *      but before the call to qagen_app_close)
 *  @note Do we really need this? The only application-scope data that is
 *      needed outside of this file is the instance handle
 */
/* struct qagen_app *qagen_app_ptr(void); */


/** @brief Fetches the application instance handle
 *  @returns The HINSTANCE for this application
 *  @note This function supersedes qagen_app_ptr because this is the only
 *      information that is needed outside of this file
 */
HINSTANCE qagen_app_instance(void);


/** @brief Displays this thread's last error in a message box. No setup is
 *      needed to invoke this function
 *  @note This function *always* draws a message box, even if there is no
 *      error currently raised! If you see an empty error message box, then you
 *      have a bug!
 */
void qagen_app_show_error(void);


#endif /* QAGEN_APP_H */
