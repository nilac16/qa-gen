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


int qagen_debug_print_stack(const CONTEXT *ectx)
{
    static const wchar_t *fmtlnno = L"   #%d: %#x <%s> (line %d)";
    static const wchar_t *fmtnoln = L"   #%d: %#x <%s>";
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    IMAGEHLP_LINEW64 line = { 0 };
    STACKFRAME64 frame = { 0 };
    DWORD64 disp = 0;
    CONTEXT record;
    BOOL res;
    int i = 0;
    SYMBOL_INFOW *syminfo = _alloca(sizeof *syminfo + sizeof *syminfo->Name * MAX_SYM_NAME);
    SymInitializeW(process, NULL, TRUE);
    memset(syminfo, 0, sizeof *syminfo);
    memcpy(&record, ectx, sizeof record);
    frame.AddrPC.Offset = ectx->Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ectx->Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ectx->Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    while (qagen_debug_stack_walk(process, thread, &frame, &record)) {
        syminfo->SizeOfStruct = sizeof *syminfo;
        syminfo->MaxNameLen = MAX_SYM_NAME - 2;
        line.SizeOfStruct = sizeof line;
        if (!SymFromAddrW(process, frame.AddrPC.Offset, &disp, syminfo)) {
            wcscpy(syminfo->Name, L"???");
        }
        res = SymGetLineFromAddrW64(process, frame.AddrPC.Offset, &(DWORD){ 0 }, &line);
        qagen_log_printf(QAGEN_LOG_DEBUG, (res) ? fmtlnno : fmtnoln,
                         i++, syminfo->Address, syminfo->Name, line.LineNumber);
    }
    return 0;
}


#define MEM_TABLE_SIZE  389
#define MEM_LOAD_CAP    256 /* Issue warnings if there are more allocs than this */

#define MEM_TRACE_LEN 4 /* The number of stack frames from the alloc site */

/** @brief Hash table used to store pointers to heap memory used by this
 *      application
 *  @details The hash function is identity (the value of the pointer), and the
 *      table is fixed size. If too many pointers are inserted, it does not
 *      react in any way, but issues warnings to log files when MEM_LOAD_CAP is
 *      exceeded
 */
static struct {
    ULONG_PTR load;
    struct {
        USHORT      nframe;
        void       *frame[MEM_TRACE_LEN];
        const void *addr;
    } table[MEM_TABLE_SIZE];
} memtable = { 0 };


void qagen_debug_memtable_insert(const void *addr)
{
    size_t hash = (ULONG_PTR)addr % BUFLEN(memtable.table);
    while (memtable.table[hash].addr) {
        if (memtable.table[hash].addr == addr) {
            qagen_log_printf(QAGEN_LOG_WARN, L"Memtable: Duplicate address %#x", addr);
            return;
        } else {
            hash = (hash + 1) % BUFLEN(memtable.table);
        }
    }
    memtable.table[hash].addr = addr;
    memtable.table[hash].nframe = CaptureStackBackTrace(2, BUFLEN(memtable.table[hash].frame), memtable.table[hash].frame, NULL);
    memtable.load++;
    if (memtable.load > MEM_LOAD_CAP) {
        qagen_log_printf(QAGEN_LOG_WARN, L"Memtable: Load factor %llu is dangerously high", memtable.load);
    }
}


/** @brief Helper function
 *  @details We need to keep searching if the pointer at @p next is hashed to
 *      a position earlier than @p idx. If there is no pointer at @p next, then
 *      we can break
 *  @param idx
 *      Index to be deleted
 *  @param next
 *      Index to be checked
 *  @returns true if we should continue searching for a replacement entry
 */
static bool qagen_debug_memtable_propcmp(size_t idx, size_t next)
{
    return memtable.table[next].addr
        && (ULONG_PTR)memtable.table[next].addr % BUFLEN(memtable.table) > idx;
}


/** @brief Find the next entry in the hash table that can be moved to @p idx
 *  @param idx
 *      Index being deleted
 */
static void qagen_debug_memtable_propagate(size_t idx)
{
    size_t next = idx;
    do {
        next = (next + 1) % BUFLEN(memtable.table);
    } while (qagen_debug_memtable_propcmp(idx, next));
    memcpy(&memtable.table[idx], &memtable.table[next], sizeof memtable.table[next]);
    if (memtable.table[next].addr) {
        qagen_debug_memtable_propagate(next);
    }
}


void qagen_debug_memtable_delete(const void *addr)
{
    size_t hash = (ULONG_PTR)addr % BUFLEN(memtable.table);
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
    size_t hash = (ULONG_PTR)addr % BUFLEN(memtable.table);
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
    HANDLE process = GetCurrentProcess();
    SYMBOL_INFOW *syminfo;
    const size_t symsize = sizeof *syminfo + sizeof *syminfo->Name * MAX_SYM_NAME;
    syminfo = _alloca(symsize);
    memset(syminfo, 0, symsize);
    SymInitializeW(process, NULL, TRUE);
    for (USHORT i = 0; i < n; i++) {
        syminfo->MaxNameLen = MAX_SYM_NAME - 2;
        syminfo->SizeOfStruct = sizeof *syminfo;
        if (!SymFromAddrW(process, (DWORD64)frame[i], 0, syminfo)) {
            wcscpy(syminfo->Name, L"???");
        }
        qagen_log_printf(QAGEN_LOG_DEBUG, L"   %#x <%s>", frame[i], syminfo->Name);
    }
}


void qagen_debug_memtable_log_extant(void)
{
    unsigned count = 0;
    for (size_t i = 0; i < BUFLEN(memtable.table); i++) {
        if (memtable.table[i].addr) {
            qagen_log_printf(QAGEN_LOG_DEBUG, L"Memtable: %#x was previously allocated at", memtable.table[i].addr);
            qagen_memtable_extant_trace(memtable.table[i].nframe, memtable.table[i].frame);
            count++;
        }
    }
    qagen_log_printf(QAGEN_LOG_DEBUG, L"Memtable: %u extant pointer%s", count, (count == 1) ? L"" : L"s");
    if (count != memtable.load) {
        qagen_log_printf(QAGEN_LOG_ERROR, L"Memtable: Extant count %u differs from load factor %u", count, memtable.load);
    }
}
