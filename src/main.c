#include "qagen-app.h"
#include "qagen-debug.h"


static int wmain_except_handler(int xcept, const CONTEXT *ectx)
{
    const wchar_t *xname = L"SEH exception propagated to main: %#x";
    switch (xcept) {
    case STATUS_ACCESS_VIOLATION:
        xname = L"Segmentation fault";
        break;
    case STATUS_DATATYPE_MISALIGNMENT:
        xname = L"Bus error";
        break;
    case STATUS_ILLEGAL_INSTRUCTION:
        xname = L"Illegal instruction";
        break;
    case STATUS_PRIVILEGED_INSTRUCTION:
        xname = L"Privileged instruction";
        break;
    case STATUS_INTEGER_DIVIDE_BY_ZERO:
        xname = L"Division by zero";
        break;
    case STATUS_STACK_BUFFER_OVERRUN:
    case STATUS_STACK_OVERFLOW:
        xname = L"Stack segment violation";
        break;
    case STATUS_HEAP_CORRUPTION:
        xname = L"Heap corrupted";
        break;
    case STATUS_BREAKPOINT:
        xname = L"Trace/breakpoint trap";
        break;
    }
    qagen_log_printf(QAGEN_LOG_ERROR, xname, xcept);
    qagen_debug_print_stack(ectx);
    return 0;
}


static int wmain_except_filter(int xcept, EXCEPTION_POINTERS *eptrs)
{
    wmain_except_handler(xcept, eptrs->ContextRecord);
    return EXCEPTION_EXECUTE_HANDLER;
}


/* int WINAPI wWinMain(HINSTANCE hinst, HINSTANCE hprev, wchar_t *cmdline, int ncmdshow) */
int wmain(void)
{
    struct qagen_app app = {
        .hinst    = GetModuleHandle(NULL),
        .cmdline  = &(wchar_t){ L'\0' },
        .ncmdshow = 1
    };
    int stat = 1;
    __try {
        stat = qagen_app_open(&app);
        if (!stat) {
            stat = qagen_app_run();
        }
        if (stat) {
            qagen_app_show_error();
        }
        qagen_app_close();
    } __except (wmain_except_filter(GetExceptionCode(), GetExceptionInformation())) {
        stat = GetExceptionCode();
    }
    return stat;
}
