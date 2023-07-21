#include <stdarg.h>
#include <stdio.h>
#include "qagen-log.h"
#include "qagen-memory.h"
#include "qagen-error.h"


static struct qagen_log_list {
    struct qagen_log_list *next;

    struct qagen_log *lf;
} *loghead = NULL;


int qagen_log_add(struct qagen_log *log)
{
    static const wchar_t *failmsg = L"Failed to allocate log file list node";
    struct qagen_log_list *node;

    node = qagen_malloc(sizeof *node);
    if (node) {
        node->next = loghead;
        node->lf = log;
        loghead = node;
    }
    return node == NULL;
}


void qagen_log_cleanup(void)
{
    struct qagen_log_list *ls = loghead, *next;

    while (ls) {
        next = ls->next;
        qagen_free(ls);
        ls = next;
    }
    loghead = NULL;
}


void qagen_log_puts(qagen_loglvl_t lvl, const wchar_t *s)
{
    struct qagen_log_list *ls;

    for (ls = loghead; ls; ls = ls->next) {
        if (lvl >= ls->lf->threshold) {
            ls->lf->callback(s, ls->lf->cbdata, lvl);
        }
    }
}


void qagen_log_printf(qagen_loglvl_t lvl, const wchar_t *restrict fmt, ...)
{
    wchar_t msg[256];
    va_list args;

    va_start(args, fmt);
    vswprintf(msg, BUFLEN(msg), fmt, args);
    va_end(args);
    qagen_log_puts(lvl, msg);
}
