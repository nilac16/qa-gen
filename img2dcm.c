#include <stdio.h>
#include "src/qagen-path.h"
#include "src/qagen-img2dcm.h"
#include "src/qagen-error.h"
#include "src/qagen-log.h"

#define PROGNAME L"img2dcm"


static void print_usage(void)
{
    fputws(L"Usage: " PROGNAME " IMG TEMPLATE\n"
           L"Convert ITK-readable image file IMG to a DICOM RTDose file, using the tags in\n"
           L"DICOM file TEMPLATE as a basis\n", stdout);
}


static int main_run(const wchar_t *src, PATH *dst, const wchar_t *tmplt)
{
    int res = 0;

    if (dst) {
        qagen_path_remove_extension(&dst);
        if (!qagen_path_rename_extension(&dst, L"dcm")) {
            if (qagen_img2dcm_convert(src, dst->buf, tmplt)) {
                res = 4;
            }
        } else {
            res = 3;
        }
        qagen_path_free(dst);
    } else {
        res = 2;
    }
    return res;
}


static int log_cb(const wchar_t *msg, void *data, qagen_loglvl_t lvl)
{
    const wchar_t *prefix = L"Info";
    FILE *fp = stdout;

    (void)data;
    switch (lvl) {
    case QAGEN_LOG_DEBUG:
        prefix = L"Debug";
        break;
    case QAGEN_LOG_INFO:
        break;
    case QAGEN_LOG_WARN:
        prefix = L"Warning";
        fp = stderr;
        break;
    case QAGEN_LOG_ERROR:
        prefix = L"Error";
        fp = stderr;
        break;
    }
    return fwprintf(stdout, PROGNAME L": %s: %s\n", prefix, msg);
}


int wmain(int argc, wchar_t *argv[])
{
    const wchar_t *erctx, *ermsg;
    struct qagen_log lf = {
        .callback  = log_cb,
        .cbdata    = NULL,
        .threshold = QAGEN_LOG_INFO
    };
    int res = 0;

    if (qagen_log_add(&lf)) {
        fputws(PROGNAME L": Error: Failed to add log file\n", stderr);
    }
    if (argc < 3) {
        qagen_log_puts(QAGEN_LOG_ERROR, L"Missing required operand");
        print_usage();
        return 1;
    }
    if ((res = main_run(argv[1], qagen_path_create(argv[1]), argv[2]))) {
        qagen_error_string(&erctx, &ermsg);
        if (ermsg[0]) {
            fwprintf(stderr, PROGNAME L": Error: %s: %s", erctx, ermsg);
        } else {
            fwprintf(stderr, PROGNAME L": Error: %s", erctx);
        }
    }
    qagen_log_cleanup();
    return res;
}
