#pragma once
/** @file Various debugging utilities (it's the bits of GDB that I just can't
 *      seem to find on Windows)
 */
#ifndef QAGEN_DEBUG_H
#define QAGEN_DEBUG_H

#include "qagen-defs.h"


/** @brief Prints the stack trace contained by @p ctx to logs
 *  @param ctx
 *      Win32 CONTEXT struct
 *  @returns Zero. The return value is not used
 *  @note This function is intended to be used from the scope of an SEH filter
 *      function. I don't know how well it will work from any other context
 *      wherein a CONTEXT object is available
 */
int qagen_debug_print_stack(const CONTEXT *ctx);


/** @brief Inserts @p addr into a static table of extant heap pointers
 *  @param addr
 *      Pointer to insert
 */
void qagen_debug_memtable_insert(const void *addr);


/** @brief Removes @p addr from the pointer table
 *  @param addr
 *      Pointer to remove
 */
void qagen_debug_memtable_delete(const void *addr);


/** @brief Gets the UID of @p addr in the pointer table
 *  @param addr
 *      Pointer to look up
 *  @returns Nonzero if found, zero if not found
 */
int qagen_debug_memtable_lookup(const void *addr);


/** @brief Logs all extant pointers in the table. If a pointer is found, up to
 *      16 stack frames from the site of the call to qagen_malloc (or other
 *      function) are sent to debug logs
 */
void qagen_debug_memtable_log_extant(void);


#endif /* QAGEN_DEBUG_H */
