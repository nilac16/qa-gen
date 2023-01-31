#include "qagen-progdlg.h"
#include "qagen-error.h"
#include "qagen-log.h"

/** C4133: So Microsoft makes an oopsie, and *I* have to listen to them
 *  complaining? */
#pragma warning(disable: 4133)

/** Invokes a superclass method on pd */
#define PDLG_SUPER(pd, name)    pd->vtbl->parent.name

/** Invokes a non-superclass method on pd */
#define PDLG_METHOD(pd, name)   pd->vtbl->cls.name


static int qagen_progdlg_create_instance(struct qagen_progdlg *pdlg)
{
    static const wchar_t *failmsg = L"Failed to create instance of ProgressDialog";
    HRESULT hr = CoCreateInstance(&CLSID_ProgressDialog,
                                  NULL,
                                  CLSCTX_INPROC_SERVER,
                                  &IID_IProgressDialog,
                                  &pdlg->pd);
    if (FAILED(hr)) {
        qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failmsg);
    }
    return FAILED(hr);
}


static int qagen_progdlg_set_title(struct qagen_progdlg *pdlg,
                                   const wchar_t        *title)
{
    static const wchar_t *failmsg = L"Failed to set progress dialog title";
    HRESULT hr = PDLG_METHOD(pdlg->pd, SetTitle)(pdlg->pd, title);
    if (FAILED(hr)) {
        qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failmsg);
    }
    return FAILED(hr);
}


static int qagen_progdlg_set_cancelmsg(struct qagen_progdlg *pdlg)
{
    static const wchar_t *failmsg = L"Failed to set progress cancel message";
    HRESULT hr = PDLG_METHOD(pdlg->pd, SetCancelMsg)(pdlg->pd, L"Stopping...", NULL);
    if (FAILED(hr)) {
        qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failmsg);
    }
    return FAILED(hr);
}


static int qagen_progdlg_init(struct qagen_progdlg *pdlg, const wchar_t *title)
{
    return qagen_progdlg_create_instance(pdlg)
        || qagen_progdlg_set_title(pdlg, title)
        || qagen_progdlg_set_cancelmsg(pdlg);
}


static int qagen_progdlg_create(struct qagen_progdlg *pdlg)
{
    static const wchar_t *failmsg = L"Failed to start progress dialog";
    HRESULT hr = PDLG_METHOD(pdlg->pd, StartProgressDialog)(pdlg->pd,
                                                            NULL,
                                                            NULL,
                                                            PROGDLG_AUTOTIME,
                                                            NULL);
    if (FAILED(hr)) {
        qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failmsg);
    }
    return FAILED(hr);
}


int qagen_progdlg_show(struct qagen_progdlg *pdlg, const wchar_t *title)
{
    if (qagen_progdlg_init(pdlg, title)
     || qagen_progdlg_create(pdlg)) {
        return 1;
    }
    return 0;
}


void qagen_progdlg_destroy(struct qagen_progdlg *pdlg)
{
    HRESULT hr = PDLG_METHOD(pdlg->pd, StopProgressDialog)(pdlg->pd);
    if (FAILED(hr)) {
        qagen_log_printf(QAGEN_ERR_HRESULT, L"ProgressDialog failed to close: HRESULT %#x", hr);
    }
    PDLG_SUPER(pdlg->pd, Release)(pdlg->pd);
}


int qagen_progdlg_set_progress(struct qagen_progdlg *pdlg,
                               ULONGLONG             completed,
                               ULONGLONG             total)
{
    static const wchar_t *failmsg = L"Failed to update ProgressDialog progress";
    HRESULT hr;
    hr = PDLG_METHOD(pdlg->pd, SetProgress64)(pdlg->pd, completed, total);
    if (FAILED(hr)) {
        qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failmsg);
    }
    return FAILED(hr);
}


int qagen_progdlg_set_line(struct qagen_progdlg *pdlg,
                           const wchar_t        *line,
                           DWORD                 lineno)
{
    static const wchar_t *failfmt = L"Failed to set progress dialog line %d";
    HRESULT hr;
    hr = PDLG_METHOD(pdlg->pd, SetLine)(pdlg->pd, lineno, line, TRUE, NULL);
    if (FAILED(hr)) {
        qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failfmt, lineno);
    }
    return FAILED(hr);
}


bool qagen_progdlg_cancelled(const struct qagen_progdlg *pdlg)
{
    return PDLG_METHOD(pdlg->pd, HasUserCancelled)(pdlg->pd);
}
