#include <stdio.h>
#include "qagen-app.h"
#include "qagen-shell.h"
#include "qagen-path.h"
#include "qagen-memory.h"
#include "qagen-log.h"
#include "qagen-error.h"
#include <CommCtrl.h>


static struct qagen_app *app = NULL;


static qagen_loglvl_t qagen_app_log_threshold(void)
{
#if _DEBUG
    return QAGEN_LOG_DEBUG;
#else
    return QAGEN_LOG_INFO;
#endif
}


static int qagen_app_init_log(void)
{
    static struct qagen_log log;

    if (!qagen_console_init(&app->cons)) {
        log.threshold = qagen_app_log_threshold();
        log.callback = (qagen_logfn_t)qagen_console_callback;
        log.cbdata = &app->cons;
        return qagen_log_add(&log);
    }
    return 1;
}


/** This is kind of a dumb hack: Because I store the exe on the network, and I
 *  don't want it to exert needless filesystem pressure on the remote server, I
 *  forbid running this from *ANY* remote drive...
 *  This is not exactly inline with my UX principles, so I need to come up with
 *  a better solution
 */
static int qagen_app_check_cwd(void)
{
    static const wchar_t *fmt = L"GetDriveType: %s";

    switch (GetDriveType(NULL)) {
    case DRIVE_UNKNOWN:
        qagen_log_printf(QAGEN_LOG_INFO, fmt, L"DRIVE_UNKNOWN");
        break;
    case DRIVE_NO_ROOT_DIR:
        qagen_log_printf(QAGEN_LOG_INFO, fmt, L"DRIVE_NO_ROOT_DIR");
        break;
    case DRIVE_REMOVABLE:
        qagen_log_printf(QAGEN_LOG_INFO, fmt, L"DRIVE_REMOVABLE");
        break;
    case DRIVE_FIXED:
        qagen_log_printf(QAGEN_LOG_INFO, fmt, L"DRIVE_FIXED");
        break;
    case DRIVE_REMOTE:
        qagen_log_printf(QAGEN_LOG_WARN, fmt, L"DRIVE_REMOTE");
        /* qagen_error_raise(QAGEN_ERR_RUNTIME, L"This program may not be run from the network", L"Please copy to local disk and try again");
        return 1; */
        break;
    case DRIVE_CDROM:
        qagen_log_printf(QAGEN_LOG_INFO, fmt, L"DRIVE_CDROM");
        break;
    case DRIVE_RAMDISK:
        qagen_log_printf(QAGEN_LOG_INFO, fmt, L"DRIVE_RAMDISK");
        break;
    }
    return 0;
}


static int qagen_app_init_cwd(void)
{
    static const wchar_t *failmsg = L"Failed to set CWD to executable path";
    PATH *exepath;
    int res;

    exepath = qagen_path_to_executable();
    res = exepath == NULL;
    if (exepath) {
        qagen_path_remove_filespec(&exepath);
        if (SetCurrentDirectory(exepath->buf)) {
            qagen_log_printf(QAGEN_LOG_INFO, L"Set CWD to %s", exepath->buf);
            res = qagen_app_check_cwd();
        } else {
            qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
            res = 1;
        }
        qagen_path_free(exepath);
    }
    return res;
}


static int qagen_app_init_comctl(void)
{
    static const wchar_t *failmsg = L"Failed to initialize Comctl32";
    INITCOMMONCONTROLSEX iccex = {
        .dwSize = sizeof iccex,
        .dwICC  = ICC_STANDARD_CLASSES
    };

    if (!InitCommonControlsEx(&iccex)) {
        qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
        return 1;
    }
    return 0;
}


static int qagen_app_init_com(void)
{
    static const wchar_t *failmsg = L"Failed to initialize COM";
    HRESULT hr;

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failmsg);
    }
    return FAILED(hr);
}


static int qagen_app_init_rpwnd(void)
{
    return qagen_rpwnd_init();
}


static int qagen_app_init(void)
{
    static int (*table[])(void) = {
        qagen_app_init_log,
        qagen_app_init_cwd,
        qagen_app_init_comctl,
        qagen_app_init_com,
        qagen_app_init_rpwnd
    };
    unsigned i;

    for (i = 0; i < BUFLEN(table); i++) {
        if (table[i]()) {
            return 1;
        }
    }
    return 0;
}


int qagen_app_open(struct qagen_app *app_ptr)
{
    app = app_ptr;
    if (qagen_app_init()) {
        return 1;
    }
    return 0;
}


static bool qagen_app_continue(void)
{
    static const wchar_t *failmsg = L"Task dialog failed to display";
    HRESULT hr;
    int res;

    hr = TaskDialog(NULL,
                    app->hinst,
                    L"Continue",
                    L"Would you like to select more patients?",
                    NULL,
                    TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
                    NULL,
                    &res);
    if (SUCCEEDED(hr)) {
        return res == IDYES;
    } else {
        qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failmsg);
        return false;
    }
}


int qagen_app_run(void)
{
    bool shouldcontinue;

    do {
        switch (qagen_shell_run()) {
        case SHELL_CLOSED:
            qagen_log_puts(QAGEN_LOG_DEBUG, L"User closed the application");
            shouldcontinue = false;
            break;
        case SHELL_ERROR:
            qagen_app_show_error();
            qagen_error_raise(QAGEN_ERR_NONE, NULL, NULL);
            /* FALLTHRU */
        case SHELL_NORMAL:
            shouldcontinue = qagen_app_continue();
        }
    } while (shouldcontinue);
    return 0;
}


void qagen_app_close(void)
{
    qagen_console_destroy(&app->cons);
    qagen_log_cleanup();
    app = NULL;
}


/* struct qagen_app *qagen_app_ptr(void)
{
    return app;
} */


HINSTANCE qagen_app_instance(void)
{
    return app->hinst;
}


void qagen_app_show_error(void)
{
    static const wchar_t *title = L"Error";
    const wchar_t *ctx, *msg;
    wchar_t buf[256];

    qagen_error_string(&ctx, &msg);
    swprintf(buf, BUFLEN(buf), L"%s\n%s", ctx, msg);
    MessageBoxEx(NULL, buf, title, MB_ICONERROR, LANG_USER_DEFAULT);
}
