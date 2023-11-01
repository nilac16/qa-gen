#pragma once
/** @file Memory management functions, mostly identical to their stdlib
 *      counterparts, but raise a thread-local error state on failure
 */
#ifndef QAGEN_MEMORY_H
#define QAGEN_MEMORY_H

#if defined(__cplusplus) || __cplusplus
extern "C" {
#endif


/** @brief Allocates at least @p size bytes of memory and returns a pointer to
 *      the block. If this function fails, it raises an error state
 *  @param size
 *      Number of bytes to allocate
 *  @returns A pointer to the allocated block, or NULL on failure
 *  @note It is implementation-defined (aka, on Windows, I have no idea)
 *      whether malloc(0) returns NULL or not
 */
void *qagen_malloc(size_t size);


/** @brief Allocates a block of @p nmemb elements, each of @p size bytes. If
 *      this function fails, it raises an error state
 *  @param nmemb
 *      Number of objects in the block
 *  @param size
 *      Size of each element in bytes
 *  @returns A pointer to the requested block, or NULL on error
 */
void *qagen_calloc(size_t nmemb, size_t size);


/** @brief Reallocates block @p addr, if it exists, to a new block of at least
 *      @p size bytes. If this function fails, it raises an error state
 *  @param addr
 *      Address to block to be reallocated. If NULL, this is equivalent to
 *      malloc(size)
 *  @param size
 *      Number of bytes to be reallocated
 *  @returns If @p size is not zero, a pointer to the newly reallocated block,
 *      or NULL on failure
 *  @todo Make sure that... Microsoft doesn't implement C23 behavior... or this
 *      function will need some attention
 */
void *qagen_realloc(void *addr, size_t size);


/** @brief Frees the memory at @p addr. This function cannot fail: It jumps
 *      directly to free(3)
 *  @param addr
 *      Block to be freed
 */
void qagen_free(void *addr);


/** @brief Securely zeroes the memory at @p addr, then frees it
 *  @param addr
 *      Block to be zeroed. May be NULL (nops if it is)
 *  @note This function uses Microsoft's _msize to fetch the size of the block
 *      before zeroing it
 */
void qagen_freezero(void *addr);


/** @brief Applies @p free_fn to pointer referenced by @p addr, then sets
 *      @p addr to NULL
 *  @param addr
 *      Pointer to pointer to be freed. The target pointer may be NULL as long
 *      as @p free_fn can deal with such case
 *  @param free_fn
 *      Function pointer applied to @p addr
 *  @note @p free_fn may not be NULL
 */
void qagen_ptr_nullify(void **addr, void (*free_fn)(void *));


#if defined(__cplusplus) || __cplusplus
}
#endif

#endif /* QAGEN_MEMORY_H */
