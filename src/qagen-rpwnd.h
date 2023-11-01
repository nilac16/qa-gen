#pragma once
/** @file A window that allows the user to select from multiple RTPlan files
 */
#ifndef QAGEN_RTPLAN_WINDOW_H
#define QAGEN_RTPLAN_WINDOW_H

#include "qagen-defs.h"


struct qagen_rpwnd {
    SIZE btnsz;

    HWND htoplevel;
    HWND hlabel;
    HWND hlist;
    HWND haccept;
};


/** @brief Registers the RTPlan list window
 *  @returns Nonzero on error
 */
int qagen_rpwnd_init(void);


enum {
    RPWND_ERROR  = -2,  /* Set error flags */
    RPWND_CLOSED = -1   /* Terminate without error */
};

/** @brief Displays the RTPlan window, and blocks until the user selects a
 *      string
 *  @param nstr
 *      Number of strings to display
 *  @param str
 *      Array of pointers to strings which are to be displayed
 *  @returns A nonnegative integer corresponding to the user's choice, or a
 *      negative integer on failure. See the above enumeration for details
 */
int qagen_rpwnd_show(struct qagen_rpwnd *wnd, int nstr, const wchar_t *str[]);


#endif /* QAGEN_RTPLAN_WINDOW_H */
