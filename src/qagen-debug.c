#include <stdint.h>
#include "qagen-debug.h"
#include "qagen-log.h"
#include <DbgHelp.h>


/** @brief A big old conditional expression that takes up too much space in its
 *      own scope
 */
static bool qagen_debug_stack_walk(HANDLE        process, HANDLE   thread,
                                   STACKFRAME64 *frame,   CONTEXT *ctx)
{
    return StackWalk64(IMAGE_FILE_MACHINE_AMD64,
                       process,
                       thread,
                       frame,
                       ctx,
                       NULL,
                       SymFunctionTableAccess64,
                       SymGetModuleBase64,
                       NULL);
}


/** @brief This takes up a lot of space */
static void qagen_debug_stackframe_init(STACKFRAME64 *frame, const CONTEXT *ctx)
{
    frame->AddrPC.Offset = ctx->Rip;
    frame->AddrPC.Mode = AddrModeFlat;
    frame->AddrStack.Offset = ctx->Rsp;
    frame->AddrStack.Mode = AddrModeFlat;
    frame->AddrFrame.Offset = ctx->Rbp;
    frame->AddrFrame.Mode = AddrModeFlat;
}


/** @brief This is reused */
static void qagen_debug_symfromaddr(HANDLE   process, const DWORD64 address,
                                    DWORD64 *disp,    SYMBOL_INFOW *syminfo)
{
    if (!SymFromAddrW(process, address, disp, syminfo)) {
        wcscpy(syminfo->Name, L"???");
    }
}


int qagen_debug_print_stack(const CONTEXT *ectx)
{
    static const wchar_t *fmtlnno = L"   #%d: %#x <%s> (line %d)";
    static const wchar_t *fmtnoln = L"   #%d: %#x <%s>";
    IMAGEHLP_LINEW64 line = { 0 };
    STACKFRAME64 frame = { 0 };
    HANDLE process, thread;
    SYMBOL_INFOW *syminfo;
    CONTEXT record;
    BOOL res;
    int i = 0;

    process = GetCurrentProcess();
    thread = GetCurrentThread();
    syminfo = _alloca(sizeof *syminfo + sizeof *syminfo->Name * MAX_SYM_NAME);
    SymInitializeW(process, NULL, TRUE);
    memset(syminfo, 0, sizeof *syminfo);
    memcpy(&record, ectx, sizeof record);
    syminfo->SizeOfStruct = sizeof *syminfo;
    syminfo->MaxNameLen = MAX_SYM_NAME - 2;
    line.SizeOfStruct = sizeof line;
    qagen_debug_stackframe_init(&frame, ectx);
    while (qagen_debug_stack_walk(process, thread, &frame, &record)) {
        qagen_debug_symfromaddr(process, frame.AddrPC.Offset, NULL, syminfo);
        res = SymGetLineFromAddrW64(process, frame.AddrPC.Offset, &(DWORD){ 0 }, &line);
        qagen_log_printf(QAGEN_LOG_DEBUG, (res) ? fmtlnno : fmtnoln,
                         i++, syminfo->Address, syminfo->Name, line.LineNumber);
    }
    return 0;
}


#define MEM_TABLE_SIZE  389
#define MEM_LOAD_CAP    292 /* Issue warnings if there are more allocs than this */

#define MEM_TRACE_LEN 4 /* The number of stack frames from the alloc site */

/** @brief Hash table used to store pointers to heap memory used by this
 *      application
 *  @details The hash function is identity (the value of the pointer), and the
 *      table is fixed size. If too many pointers are inserted, it does not
 *      react in any way, but issues warnings to log files when MEM_LOAD_CAP is
 *      exceeded
 */
static struct {
    ULONG_PTR psl_len, psl_net;

    ULONG_PTR load;
    struct {
        unsigned    psl;
        USHORT      nframe;
        void       *frame[MEM_TRACE_LEN];
        const void *addr;
    } table[MEM_TABLE_SIZE];
} memtable = { 0 };


/** @brief Simple pointer hash function
 *  @returns A hash of @p addr
 */
static size_t qagen_debug_memtable_hash(const void *addr)
{
    const size_t prime = INT32_MAX;
    size_t res = (size_t)addr * prime;

    return res;
}


void qagen_debug_memtable_insert(const void *addr)
{
    unsigned psl = 0;
    size_t hash;

    hash = qagen_debug_memtable_hash(addr) % BUFLEN(memtable.table);
    while (memtable.table[hash].addr) {
        if (memtable.table[hash].addr == addr) {
            qagen_log_printf(QAGEN_LOG_WARN, L"Memtable: Duplicate address %#x", addr);
            return;
        } else {
            hash = (hash + 1) % BUFLEN(memtable.table);
            psl++;
        }
    }
    memtable.psl_net += psl;
    memtable.psl_len++;
    memtable.table[hash].psl = psl;
    memtable.table[hash].addr = addr;
    memtable.table[hash].nframe = CaptureStackBackTrace(2, BUFLEN(memtable.table[hash].frame), memtable.table[hash].frame, NULL);
    memtable.load++;
    if (memtable.load > MEM_LOAD_CAP) {
        qagen_log_printf(QAGEN_LOG_WARN, L"Memtable: Load factor %u is dangerously high", memtable.load);
    }
}


/** @brief Find the next entry in the hash table that can be moved to @p idx
 *  @param idx
 *      Index being deleted
 */
static void qagen_debug_memtable_propagate(size_t idx)
{
    size_t next = idx;
    unsigned psl = 0;

    do {
        next = (next + 1) % BUFLEN(memtable.table);
        psl++;
    } while (memtable.table[next].addr && psl > memtable.table[next].psl);

    memcpy(&memtable.table[idx], &memtable.table[next], sizeof memtable.table[next]);
    memtable.table[idx].psl -= psl;
    if (memtable.table[next].addr) {
        qagen_debug_memtable_propagate(next);
    }
}


void qagen_debug_memtable_delete(const void *addr)
{
    size_t hash;

    hash = qagen_debug_memtable_hash(addr) % BUFLEN(memtable.table);
    while (memtable.table[hash].addr) {
        if (memtable.table[hash].addr == addr) {
            qagen_debug_memtable_propagate(hash);
            memtable.load--;
            return;
        } else {
            hash = (hash + 1) % BUFLEN(memtable.table);
        }
    }
    qagen_log_printf(QAGEN_LOG_WARN, L"Memtable: Address %#x was not found", addr);
}


int qagen_debug_memtable_lookup(const void *addr)
{
    size_t hash;

    hash = qagen_debug_memtable_hash(addr) % BUFLEN(memtable.table);
    while (memtable.table[hash].addr) {
        if (memtable.table[hash].addr == addr) {
            return 1;
        } else {
            hash = (hash + 1) % BUFLEN(memtable.table);
        }
    }
    qagen_log_printf(QAGEN_LOG_WARN, L"Memtable: Address %#x was not found", addr);
    return 0;
}


/** @brief Prints the stack frames in @p frame to debug logs
 *  @param n
 *      Number of frames stored
 *  @param frame
 *      Array of void pointers to system-dependent stack frame information.
 *      This information should have been obtained by a previous call to
 *      CaptureStackBackTrace(2)
 */
static void qagen_memtable_extant_trace(USHORT n, void *frame[])
{
    SYMBOL_INFOW *syminfo;
    HANDLE process;
    USHORT i;

    process = GetCurrentProcess();
    syminfo = _alloca(sizeof *syminfo + sizeof *syminfo->Name * MAX_SYM_NAME);
    SymInitializeW(process, NULL, TRUE);
    memset(syminfo, 0, sizeof *syminfo);
    syminfo->MaxNameLen = MAX_SYM_NAME - 2;
    syminfo->SizeOfStruct = sizeof *syminfo;
    for (i = 0; i < n; i++) {
        qagen_debug_symfromaddr(process, (DWORD64)frame[i], NULL, syminfo);
        qagen_log_printf(QAGEN_LOG_DEBUG, L"   %#x <%s>", frame[i], syminfo->Name);
    }
}


void qagen_debug_memtable_log_extant(void)
{
    unsigned count = 0, i;

    for (i = 0; i < BUFLEN(memtable.table); i++) {
        if (memtable.table[i].addr) {
            qagen_log_printf(QAGEN_LOG_DEBUG, L"Memtable: %#x was previously allocated at", memtable.table[i].addr);
            qagen_memtable_extant_trace(memtable.table[i].nframe, memtable.table[i].frame);
            count++;
        }
    }
    qagen_log_printf(QAGEN_LOG_DEBUG, L"Memtable: %u extant pointer%s", count, PLFW(count));
    qagen_log_printf(QAGEN_LOG_DEBUG, L"Memtable: Average probe sequence length: %.2f across %u insertions", (double)memtable.psl_net / (double)memtable.psl_len, memtable.psl_len);
    if (count != memtable.load) {
        qagen_log_printf(QAGEN_LOG_ERROR, L"Memtable: Extant count %u differs from load factor %u", count, memtable.load);
    }
}
