#include <stdlib.h>
#define QAGEN_MEMORY_IMPLEMENTATION_FILE
#include "qagen-memory.h"
#include "qagen-log.h"
#include "qagen-debug.h"
#include "qagen-error.h"


/** I *highly* suspect that I can use Microsoft's default allocator for this
 *  application
 */
/* static HANDLE hheap = INVALID_HANDLE_VALUE; */


void *qagen_malloc(size_t size)
{
    static const wchar_t *failfmt = L"Failed to allocate block of size %zu";
    void *res = malloc(size);
    if (!res && size) {
        qagen_error_raise(QAGEN_ERR_SYSTEM, NULL, failfmt, size);
    } else if (res) {
        qagen_debug_memtable_insert(res);
    }
    return res;
}


void *qagen_calloc(size_t nmemb, size_t size)
{
    static const wchar_t *failfmt = L"Failed to allocate %zu elements of %zu bytes";
    void *res = calloc(nmemb, size);
    if (!res && size && nmemb) {
        qagen_error_raise(QAGEN_ERR_SYSTEM, NULL, failfmt, nmemb, size);
    } else if (res) {
        qagen_debug_memtable_insert(res);
    }
    return res;
}


void *qagen_realloc(void *addr, size_t size)
{
    static const wchar_t *failfmt = L"Failed to reallocate block to size %zu";
    void *res = realloc(addr, size);
    if (!res && size) {
        qagen_error_raise(QAGEN_ERR_SYSTEM, NULL, failfmt, size);
    } else {
        if (addr) {
            qagen_debug_memtable_delete(addr);
        }
        if (res) {
            qagen_debug_memtable_insert(res);
        }
    }
    return res;
}


void qagen_free(void *addr)
{
    free(addr);
    if (addr) {
        qagen_debug_memtable_delete(addr);
    }
}


void qagen_freezero(void *addr)
{
    if (addr) {
        SecureZeroMemory(addr, _msize(addr));
        qagen_free(addr);
    }
}


void qagen_ptr_nullify(void **addr, void (*free_fn)(void *))
{
    free_fn(*addr);
    *addr = NULL;
}
