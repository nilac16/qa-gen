/** Fixes to the DICOM exporter have rendered this file defunct */
#include "qagen-app.h"
#include "qagen-log.h"
#include "qagen-error.h"
#include <CommCtrl.h>

#pragma warning(disable: 4100)

#define IDRPWND_LBOX    101
#define IDRPWND_ACCEPT  102


static ATOM class_atom = 0;


static const wchar_t *qagen_rpwnd_classname(void)
{
    return L"RPListWnd";
}


static struct qagen_rpwnd *qagen_rpwnd_context(HWND hwnd)
{
    return (struct qagen_rpwnd *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}


static void qagen_rpwnd_post_quit(HWND hwnd)
{
    PostMessage(hwnd, WM_QUIT, RPWND_CLOSED, 0);
}


static void qagen_rpwnd_post_error(HWND hwnd)
{
    PostMessage(hwnd, WM_QUIT, RPWND_ERROR, 0);
}


static void qagen_rpwnd_post_choice(struct qagen_rpwnd *wnd)
{
    LRESULT sel = SendMessage(wnd->hlist, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR) {
        PostMessage(wnd->htoplevel, WM_QUIT, sel, 0);
    }
}


static LRESULT CALLBACK qagen_rpwnd_choice_made(HWND   hwnd,   UINT   msg,
                                                WPARAM wparam, LPARAM lparam)
{
    qagen_rpwnd_post_choice(qagen_rpwnd_context(hwnd));
    return 0;
}


static int qagen_rpwnd_create_label(struct qagen_rpwnd *wnd,
                                    HINSTANCE           hinst,
                                    HWND                hwnd)
{
    static const wchar_t *failmsg = L"Failed to create RP window listbox label";
    static const wchar_t *label = L"Found multiple RTPlan files\n"
                    L"Please select the list containing the correct fields";
    wnd->hlabel = CreateWindow(WC_STATIC,
                               label,
                               WS_CHILD | WS_VISIBLE | SS_SUNKEN,
                               0, 0, 10, 10,
                               hwnd,
                               NULL,
                               hinst,
                               NULL);
    if (!wnd->hlabel) {
        qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
    }
    return wnd->hlabel == NULL;
}


static int qagen_rpwnd_create_listbox(struct qagen_rpwnd *wnd,
                                      HINSTANCE           hinst,
                                      HWND                hwnd)
{
    static const wchar_t *failmsg = L"Failed to create RP window listbox";
    static const wchar_t *title = L"";
    wnd->hlist = CreateWindow(WC_LISTBOX,
                              title,
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY,
                              0, 0, 10, 10,
                              hwnd,
                              (HMENU)IDRPWND_LBOX,
                              hinst,
                              NULL);
    if (!wnd->hlist) {
        qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
    }
    return wnd->hlist == NULL;
}


/** Retrieves the ideal size of the accept button and stores it in the relevant
 *  SIZE struct
 */
static int qagen_rpwnd_button_size(struct qagen_rpwnd *wnd)
{
    static const wchar_t *failmsg = L"BCM_GETIDEALSIZE failed";
    wnd->btnsz.cx = wnd->btnsz.cy = 0;
    if (!SendMessage(wnd->haccept, BCM_GETIDEALSIZE, 0, (LPARAM)&wnd->btnsz)) {
        qagen_error_raise(QAGEN_ERR_RUNTIME, NULL, failmsg);
        return 1;
    } else {
        return 0;
    }
}


static int qagen_rpwnd_create_buttons(struct qagen_rpwnd *wnd,
                                      HINSTANCE           hinst,
                                      HWND                hwnd)
{
    static const wchar_t *failmsg = L"Failed to create RP window buttons";
    static const wchar_t *label = L"Accept selection";
    wnd->haccept = CreateWindow(WC_BUTTON,
                                label,
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                0, 0, 10, 10,
                                hwnd,
                                (HMENU)IDRPWND_ACCEPT,
                                hinst,
                                NULL);
    if (wnd->haccept) {
        return qagen_rpwnd_button_size(wnd);
    } else {
        qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
        return 1;
    }
}


static int qagen_rpwnd_set_font(struct qagen_rpwnd *wnd)
{
    static const wchar_t *failmsg = L"Failed to fetch nonclient area scalable metrics";
    NONCLIENTMETRICS mt = { .cbSize = sizeof mt };
    HFONT font;
    if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &mt, 0)) {
        font = CreateFontIndirect(&mt.lfMessageFont);
        SendMessage(wnd->hlabel, WM_SETFONT, (WPARAM)font, 0);
        SendMessage(wnd->hlist, WM_SETFONT, (WPARAM)font, 0);
        SendMessage(wnd->haccept, WM_SETFONT, (WPARAM)font, 0);
        return 0;
    } else {
        qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
        return 1;
    }
}


static LRESULT CALLBACK qagen_rpwnd_create(HWND   hwnd,   UINT   msg,
                                           WPARAM wparam, LPARAM lparam)
{
    CREATESTRUCT *const cstruct = (CREATESTRUCT *)lparam;
    struct qagen_rpwnd *const wnd = (struct qagen_rpwnd *)cstruct->lpCreateParams;
    HINSTANCE inst = qagen_app_instance();
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)wnd);
    if (qagen_rpwnd_create_label(wnd, inst, hwnd)
     || qagen_rpwnd_create_listbox(wnd, inst, hwnd)
     || qagen_rpwnd_create_buttons(wnd, inst, hwnd)
     || qagen_rpwnd_set_font(wnd)) {
        return -1;
    }
    return 0;
}


struct layout {
    int lblx, lbly, lblw, lblh;
    int lstx, lsty, lstw, lsth;
    int btnx, btny, btnw, btnh;
};


static void qagen_rpwnd_layout(struct qagen_rpwnd *wnd, struct layout *lt, WORD w, WORD h)
{
    lt->btnx = w / 2 - wnd->btnsz.cx / 2;
    lt->btny = (h -= (2 * (WORD)wnd->btnsz.cy)) + wnd->btnsz.cy / 2;
    lt->btnw = wnd->btnsz.cx;
    lt->btnh = wnd->btnsz.cy;

    lt->lsth = (7 * h) / 8;
    lt->lstw = w;
    lt->lstx = 0;
    lt->lsty = h -= lt->lsth;

    lt->lblx = 0;
    lt->lbly = 0;
    lt->lblw = w;
    lt->lblh = h;
}


static LRESULT CALLBACK qagen_rpwnd_size(HWND   hwnd,   UINT   msg,
                                         WPARAM wparam, LPARAM lparam)
{
    struct qagen_rpwnd *wnd = qagen_rpwnd_context(hwnd);
    struct layout lt;
    qagen_rpwnd_layout(wnd, &lt, LOWORD(lparam), HIWORD(lparam));
    MoveWindow(wnd->hlabel,  lt.lblx, lt.lbly, lt.lblw, lt.lblh, TRUE);
    MoveWindow(wnd->hlist,   lt.lstx, lt.lsty, lt.lstw, lt.lsth, TRUE);
    MoveWindow(wnd->haccept, lt.btnx, lt.btny, lt.btnw, lt.btnh, TRUE);
    return 0;
}


static LRESULT CALLBACK qagen_rpwnd_close(HWND   hwnd,   UINT   msg,
                                          WPARAM wparam, LPARAM lparam)
{
    qagen_rpwnd_post_quit(hwnd);
    return 0;
}


static LRESULT CALLBACK qagen_rpwnd_keyup(HWND   hwnd,   UINT   msg,
                                          WPARAM wparam, LPARAM lparam)
{
    switch (wparam) {
    case VK_RETURN:
        return qagen_rpwnd_choice_made(hwnd, msg, wparam, lparam);
    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}


static LRESULT CALLBACK qagen_rpwnd_command_errspace(HWND   hwnd,   UINT   msg,
                                                     WPARAM wparam, LPARAM lparam)
{
    static const wchar_t *failmsg = L"WM_COMMAND::LBN_ERRSPACE: Not enough memory";
    qagen_error_raise(QAGEN_ERR_RUNTIME, NULL, failmsg);
    qagen_rpwnd_post_error(hwnd);
    return 0;
}


static LRESULT CALLBACK qagen_rpwnd_command_lbox(HWND   hwnd,   UINT   msg,
                                                 WPARAM wparam, LPARAM lparam)
{
    switch (HIWORD(wparam)) {
    case LBN_ERRSPACE:
        return qagen_rpwnd_command_errspace(hwnd, msg, wparam, lparam);
    case LBN_DBLCLK:
        return qagen_rpwnd_choice_made(hwnd, msg, wparam, lparam);
    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}


static LRESULT CALLBACK qagen_rpwnd_command_accept(HWND   hwnd,   UINT   msg,
                                                   WPARAM wparam, LPARAM lparam)
{
    switch (HIWORD(wparam)) {
    case BN_CLICKED:
        return qagen_rpwnd_choice_made(hwnd, msg, wparam, lparam);
    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}


static LRESULT CALLBACK qagen_rpwnd_command(HWND   hwnd,   UINT   msg,
                                            WPARAM wparam, LPARAM lparam)
{
    switch (LOWORD(wparam)) {
    case IDRPWND_LBOX:
        return qagen_rpwnd_command_lbox(hwnd, msg, wparam, lparam);
    case IDRPWND_ACCEPT:
        return qagen_rpwnd_command_accept(hwnd, msg, wparam, lparam);
    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}


static LRESULT CALLBACK qagen_rpwnd_proc(HWND   hwnd,   UINT   msg,
                                         WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_CREATE:
        return qagen_rpwnd_create(hwnd, msg, wparam, lparam);
    case WM_SIZE:
        return qagen_rpwnd_size(hwnd, msg, wparam, lparam);
    case WM_CLOSE:
        return qagen_rpwnd_close(hwnd, msg, wparam, lparam);
    case WM_KEYUP:
        return qagen_rpwnd_keyup(hwnd, msg, wparam, lparam);
    case WM_COMMAND:
        return qagen_rpwnd_command(hwnd, msg, wparam, lparam);
    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}


static HCURSOR qagen_load_cursor(void)
{
    static const wchar_t *failmsg = L"Failed to load cursor for RP window";
    HCURSOR res = LoadCursor(NULL, IDC_ARROW);
    if (!res) {
        qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
    }
    return res;
}


static HBRUSH qagen_load_brush(void)
{
    static const wchar_t *failmsg = L"Failed to load background brush for RP window";
    HBRUSH res = CreateBrushIndirect(&(const LOGBRUSH){
        .lbStyle = BS_SOLID,
        .lbColor = RGB(0xCC, 0xCC, 0xCC),
        .lbHatch = 0
    });
    if (!res) {
        qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
    }
    return res;
}


static int qagen_rpwnd_register(void)
{
    static const wchar_t *failmsg = L"Failed to register RTPlan list window class";
    WNDCLASSEX wcex = {
        .cbSize        = sizeof wcex,
        .style         = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc   = qagen_rpwnd_proc,
        .cbClsExtra    = 0,
        .cbWndExtra    = 0,
        .hInstance     = qagen_app_instance(),
        .hIcon         = NULL,
        .hCursor       = qagen_load_cursor(),
        .hbrBackground = qagen_load_brush(),
        .lpszMenuName  = NULL,
        .lpszClassName = qagen_rpwnd_classname(),
        .hIconSm       = NULL
    };
    class_atom = RegisterClassEx(&wcex);
    if (!class_atom) {
        if (!qagen_error_state()) {
            qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
        }
        return 1;
    } else {
        return 0;
    }
}


int qagen_rpwnd_init(void)
{
    if (qagen_rpwnd_register()) {
        return 1;
    }
    return 0;
}


void qagen_rpwnd_destroy(struct qagen_rpwnd *wnd)
{
    DestroyWindow(wnd->htoplevel);
}


static int qagen_rpwnd_create_window(struct qagen_rpwnd *wnd)
{
    static const wchar_t *failmsg = L"Failed to create RTPlan window";
    static const wchar_t *caption = L"Select RTPlan";
    wnd->htoplevel = CreateWindow((const wchar_t *)class_atom,
                                  caption,
                                  WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  400, 400,
                                  NULL, NULL,
                                  qagen_app_instance(), wnd);
    if (!wnd->htoplevel && !qagen_error_state()) {
        qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
    }
    return wnd->htoplevel == NULL;
}


static int qagen_rpwnd_get_strings(struct qagen_rpwnd *wnd,
                                   int                 nstr,
                                   const wchar_t      *str[])
{
    static const wchar_t *failmsg = L"Failed to add string to listbox";
    do {
        switch (SendMessage(wnd->hlist, LB_ADDSTRING, 0, (LPARAM)*str++)) {
        case LB_ERR:
        case LB_ERRSPACE:
            qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
            return 1;
        }
    } while (--nstr);
    return 0;
}


static void qagen_rpwnd_remove_strings(struct qagen_rpwnd *wnd, int nstr)
{
    do {
        SendMessage(wnd->hlist, LB_DELETESTRING, --nstr, 0);
    } while (nstr);
}


static int qagen_rpwnd_wait(struct qagen_rpwnd *wnd)
{
    MSG msg;
    while (GetMessage(&msg, wnd->htoplevel, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}


int qagen_rpwnd_show(struct qagen_rpwnd *wnd, int nstr, const wchar_t *str[])
{
    int res = RPWND_ERROR;
    if (!(qagen_rpwnd_create_window(wnd)
       || qagen_rpwnd_get_strings(wnd, nstr, str))) {
        ShowWindow(wnd->htoplevel, 1);
        res = qagen_rpwnd_wait(wnd);
        qagen_rpwnd_destroy(wnd);
    }
    qagen_log_printf(QAGEN_LOG_DEBUG, L"RPWND result: %d", res);
    return res;
}
