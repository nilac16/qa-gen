#include "qagen-filedlg.h"
#include "qagen-error.h"


static int qagen_filedlg_create_instance(struct qagen_filedlg *fdlg)
{
    static const wchar_t *failmsg = L"Failed to create instance of FileOpenDialog";
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_FileOpenDialog,
                          NULL,
                          CLSCTX_INPROC_SERVER,
                          &IID_IFileOpenDialog,
                          &fdlg->fd);
    if (FAILED(hr)) {
        qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failmsg);
    }
    return FAILED(hr);
}


static int qagen_filedlg_set_title(struct qagen_filedlg *fdlg)
{
    static const wchar_t *failfmt = L"Failed to set FileDialog %s";
    static const wchar_t *title = L"Select patient RS directories";
    HRESULT hr;

    hr = fdlg->fd->lpVtbl->SetTitle(fdlg->fd, title);
    if (FAILED(hr)) {
        qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failfmt, L"title");
    }
    return FAILED(hr);
}


static int qagen_filedlg_set_options(struct qagen_filedlg *fdlg)
{
    static const wchar_t *failfmt = L"Failed to set FileDialog %s";
    const FILEOPENDIALOGOPTIONS opt = FOS_PICKFOLDERS | FOS_ALLOWMULTISELECT;
    HRESULT hr;

    hr = fdlg->fd->lpVtbl->SetOptions(fdlg->fd, opt);
    if (FAILED(hr)) {
        qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failfmt, L"options");
    }
    return FAILED(hr);
}


static int qagen_filedlg_init(struct qagen_filedlg *dlg)
{
    if (qagen_filedlg_create_instance(dlg)
     || qagen_filedlg_set_title(dlg)
     || qagen_filedlg_set_options(dlg)) {
        return FILEDLG_ERROR;
    } else {
        return FILEDLG_NORMAL;
    }
}


static int qagen_filedlg_get_results(struct qagen_filedlg *dlg)
{
    static const wchar_t *failmsg = L"Failed to get FileDialog results";
    HRESULT hr;

    hr = dlg->fd->lpVtbl->GetResults(dlg->fd, &dlg->siarr);
    if (SUCCEEDED(hr)) {
        hr = dlg->siarr->lpVtbl->GetCount(dlg->siarr, &dlg->count);
        if (SUCCEEDED(hr)) {
            return FILEDLG_NORMAL;
        } else {
            qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failmsg);
        }
    } else {
        qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failmsg);
    }
    return FILEDLG_ERROR;
}


static int qagen_filedlg_wait(struct qagen_filedlg *dlg)
{
    HRESULT hr;

    hr = dlg->fd->lpVtbl->Show(dlg->fd, NULL);
    if (SUCCEEDED(hr)) {
        return qagen_filedlg_get_results(dlg);
    } else {
        return FILEDLG_CLOSED;
    }
}


int qagen_filedlg_show(struct qagen_filedlg *fdlg)
{
    int res;

    res = qagen_filedlg_init(fdlg);
    if (!res) {
        res = qagen_filedlg_wait(fdlg);
    }
    return res;
}


void qagen_filedlg_destroy(struct qagen_filedlg *dlg)
{
    if (dlg->si) {
        dlg->si->lpVtbl->Release(dlg->si);
    }
    if (dlg->siarr) {
        dlg->siarr->lpVtbl->Release(dlg->siarr);
    }
    if (dlg->fd) {
        dlg->fd->lpVtbl->Release(dlg->fd);
    }
}


static int qagen_filedlg_get_sigdn(IShellItem           *si,
                                   SIGDN                 sigdn,
                                   wchar_t             **dst)
{
    static const wchar_t *failmsg = L"Failed to get path string from ShellItem";
    HRESULT hr;

    hr = si->lpVtbl->GetDisplayName(si, sigdn, dst);
    if (FAILED(hr)) {
        qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failmsg);
    }
    return FAILED(hr);
}


int qagen_filedlg_path(struct qagen_filedlg *fdlg,
                       DWORD                 idx,
                       wchar_t    *restrict *path,
                       wchar_t    *restrict *dpy)
{
    static const wchar_t *failfmt = L"Failed to get path index %lu from FileDialog";
    HRESULT hr;

    if (fdlg->si) {
        fdlg->si->lpVtbl->Release(fdlg->si);
    }
    hr = fdlg->siarr->lpVtbl->GetItemAt(fdlg->siarr, idx, &fdlg->si);
    if (SUCCEEDED(hr)) {
        return qagen_filedlg_get_sigdn(fdlg->si, SIGDN_FILESYSPATH, path)
            || qagen_filedlg_get_sigdn(fdlg->si, SIGDN_NORMALDISPLAY, dpy);
    } else {
        qagen_error_raise(QAGEN_ERR_HRESULT, &hr, failfmt, idx);
        return 1;
    }
}
