#include <stdio.h>
#include "qagen-console.h"
#include "qagen-error.h"


int qagen_console_init(struct qagen_console *cons)
{
    static const wchar_t *failmsg = L"Failed to console output handle";
    AllocConsole(); /* This call is redundant now that I no longer use WinMain */
    cons->hcons = GetStdHandle(STD_OUTPUT_HANDLE);
    if (cons->hcons != INVALID_HANDLE_VALUE) {
        return 0;
    }
    qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
    return 1;
}


void qagen_console_destroy(struct qagen_console *cons)
{
    (void)cons;
    FreeConsole();
}


/** Gets the color for this logging level */
static WORD qagen_console_color(qagen_loglvl_t lvl)
{
    static const WORD color[] = {
        FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
        FOREGROUND_RED | FOREGROUND_INTENSITY
    };
    return color[lvl];
}


/** Gets the prefix for this logging level */
static const wchar_t *qagen_console_lvlprefix(qagen_loglvl_t lvl)
{
    static const wchar_t *prefix[] = {
        L"Debug",
        L"Info",
        L"Warning",
        L"Error"
    };
    return prefix[lvl];
}


/** Writes prefixed output to the console */
static int qagen_console_write(const wchar_t *restrict msg,
                               struct qagen_console   *cons,
                               qagen_loglvl_t          lvl)
{
    static const wchar_t *msgfmt = L"QAGen: %s: %s\n";
    wchar_t buf[256];
    DWORD nchars, nwrit;
    nchars = swprintf(buf, BUFLEN(buf), msgfmt, qagen_console_lvlprefix(lvl), msg);
    if (nchars >= 0) {
        WriteConsole(cons->hcons, buf, nchars, &nwrit, NULL);
    }
    return nchars < 0;
}


int qagen_console_callback(const wchar_t *restrict msg,
                           struct qagen_console   *cons,
                           qagen_loglvl_t          lvl)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    int res;
    if (GetConsoleScreenBufferInfo(cons->hcons, &info)) {
        SetConsoleTextAttribute(cons->hcons, qagen_console_color(lvl));
        res = qagen_console_write(msg, cons, lvl);
        SetConsoleTextAttribute(cons->hcons, info.wAttributes);
    } else {
        res = qagen_console_write(msg, cons, lvl);
    }
    return res;
}
