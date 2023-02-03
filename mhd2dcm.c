#include <stdio.h>
#include "src/qagen-path.h"
#include "src/qagen-metaio.h"
#include "src/qagen-error.h"
#include "src/qagen-log.h"


static void print_usage(void)
{
    fputws(L"Usage: mhd2dcm MHD TEMPLATE\n"
           L"Convert MetaImage header file MHD to a DICOM RTDose file, using the tags in\n"
           L"DICOM file TEMPLATE as a basis\n", stdout);
}


static int main_run(const wchar_t *src, PATH *dst, const wchar_t *tmplt)
{
    int res = 0;
    if (dst) {
        if (!qagen_path_rename_extension(&dst, L"dcm")) {
            if (qagen_metaio_convert(src, dst->buf, tmplt)) {
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
    static const wchar_t *progname = L"mhd2dcm";
    const wchar_t *prefix = L"Info";
    FILE *fp = stdout;
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
    return fwprintf(stdout, L"%s: %s: %s\n", progname, prefix, msg);
}


int wmain(int argc, wchar_t *argv[])
{
    int res = 0;
    if (qagen_log_add(&(struct qagen_log){ .callback = log_cb, .cbdata = NULL, .threshold = QAGEN_LOG_INFO })) {
        fputws(L"mhd2dcm: Error: Failed to add log file\n", stderr);
    }
    if (argc < 3) {
        qagen_log_puts(QAGEN_LOG_ERROR, L"Missing required operand");
        print_usage();
        return 1;
    }
    if ((res = main_run(argv[1], qagen_path_create(argv[1]), argv[2]))) {
        wchar_t *erctx, *ermsg;
        qagen_error_string(&erctx, &ermsg);
        if (ermsg[0]) {
            fwprintf(stderr, L"mhd2dcm: Error: %s: %s", erctx, ermsg);
        } else {
            fwprintf(stderr, L"mhd2dcm: Error: %s", erctx);
        }
    }
    qagen_log_cleanup();
    return res;
}
