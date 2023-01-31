#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include "qagen-error.h"
#include "qagen-log.h"


thread_local struct qagen_error error = { 0 };


static void qagen_error_win32(const DWORD *data)
{
    wchar_t sysbuf[128];
    error.dwerr = (data) ? *data : GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL,
                  error.dwerr,
                  LANG_USER_DEFAULT,
                  sysbuf,
                  BUFLEN(sysbuf),
                  NULL);
    swprintf(error.message, BUFLEN(error.message), L"%#x: %s", error.dwerr, sysbuf);
}


static void qagen_error_hresult(const HRESULT *hr)
{
    wchar_t sysbuf[128];
    error.hr = *hr;
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL,
                  HRESULT_CODE(error.hr),
                  LANG_USER_DEFAULT,
                  sysbuf,
                  BUFLEN(sysbuf),
                  NULL);
    swprintf(error.message, BUFLEN(error.message), L"HRESULT %#x: %s", error.hr, sysbuf);
}


static void qagen_error_system(const errno_t *errnum)
{
    error.errnum = (errnum) ? *errnum : errno;
    _wcserror_s(error.message, BUFLEN(error.message), error.errnum);
}


static void qagen_error_runtime(const wchar_t *message)
{
    if (message) {
        swprintf(error.context, BUFLEN(error.context), L"%s", message);
    }
}


void qagen_error_raise(int type, const void *data, const wchar_t *restrict fmt, ...)
{
    va_list args;
    memset(&error, 0, sizeof error);
    error.type = type;
    if (type) {
        qagen_log_printf(QAGEN_LOG_DEBUG, L"Error state %d raised", type);
    }
    switch (type) {
    case QAGEN_ERR_NONE:
        qagen_log_puts(QAGEN_LOG_DEBUG, L"Error state cleared");
        return;
    case QAGEN_ERR_WIN32:
        qagen_error_win32(data);
        break;
    case QAGEN_ERR_HRESULT:
        qagen_error_hresult(data);
        break;
    case QAGEN_ERR_SYSTEM:
        qagen_error_system(data);
        break;
    case QAGEN_ERR_RUNTIME:
        qagen_error_runtime(data);
        va_start(args, fmt);
        vswprintf(error.message, BUFLEN(error.message), fmt, args);
        va_end(args);
        return; /* Unclean hack */
    }
    va_start(args, fmt);
    vswprintf(error.context, BUFLEN(error.context), fmt, args);
    va_end(args);
}


void qagen_error_string(const wchar_t **ctx, const wchar_t **msg)
{
    *ctx = error.context;
    *msg = error.message;
}


bool qagen_error_state(void)
{
    return error.type != QAGEN_ERR_NONE;
}
