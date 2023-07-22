#pragma once
/** @file A path object that maintains both its string length, and buffer size.
 *      It is essentially an implementation of std::vector<wchar_t>
 */
#ifndef QAGEN_PATH_H
#define QAGEN_PATH_H

#include "qagen-defs.h"


typedef struct qagen_path {
    size_t  pathlen; /* The number of wchars in the string, excluding the nul */
    size_t  buflen;  /* The number of wchars reserved (buffer *count*, not size) */
    wchar_t buf[];   /* The path string */
} PATH;


/** @brief Creates a PATH structure from a raw string
 *  @param path
 *      Path to be converted
 *  @returns A PATH object. This should be freed with qagen_path_free when no
 *      longer needed
 */
PATH *qagen_path_create(const wchar_t *path);


/** @brief Duplicates a path
 *  @param path
 *      Path to copy
 *  @returns A duplicate of @p path, or NULL on error
 */
PATH *qagen_path_duplicate(const PATH *path);


/** @brief Frees memory held by @p path
 *  @param path
 *      PATH structure to delete
 */
void qagen_path_free(PATH *path);


/** @brief Joins @p ext to @p root
 *  @param root
 *      Path root
 *  @param ext
 *      Additional path to attach
 *  @returns Nonzero on error. In case of error, @p root will be in an
 *      indeterminate state, but it is guaranteed that you may pass it to
 *      qagen_path_free
 */
int qagen_path_join(PATH **root, const wchar_t *ext);


/** @brief Removes the topmost element of the @p path, if possible
 *  @param path
 *      Path to be modified
 *  @note The result of this operation is not checked internally, because I
 *      assume that it cannot really fail...
 */
void qagen_path_remove_filespec(PATH **path);


/** @brief Removes extensions, until no further removal is possible. This is
 *      intended for filenames with compound extensions (like nii.gz). If your
 *      goal is to replace the extension of a path that contains a simple
 *      extension, use qagen_path_rename_extension
 *  @param path
 *      Path to be modified
 *  @note This function fails silently. Odd behavior with output paths may trace
 *      back to this
 */
void qagen_path_remove_extension(PATH **path);


/** @brief Replaces or adds an extension present in @p path to @p ext
 *  @param path
 *      Path to be modified
 *  @param ext
 *      Extension string. Should not be prefixed with a '.', unless that is a
 *      part of the extension, I suppose...
 *  @returns Nonzero on error
 */
int qagen_path_rename_extension(PATH **path, const wchar_t *ext);


/** @brief Determines if the path fragment contained by @p fdata refers to a
 *      subdirectory (i.e. not a file, and not current/parent directory)
 *  @param fdata
 *      WIN32_FIND_DATA used for searching subdirectories
 *  @returns true if the currently contained file is a subdirectory
 *  @note Is there any system on which current dir isn't '.', and parent dir is
 *      not '..'?
 */
bool qagen_path_is_subdirectory(const WIN32_FIND_DATA *fdata);


/** @brief Fetches the executable path in a heap-allocated string
 *  @returns The exectutable path, or NULL on error. Free this with qagen_free
 */
PATH *qagen_path_to_executable(void);


/** @brief Determines whether the passed character is a valid path component on
 *      NTFS filesystems
 *  @param chr
 *      Character
 *  @returns true if @p chr is a valid path component, false otherwise
 */
bool qagen_path_char_isvalid(wchar_t chr);


#endif /* QAGEN_PATH_H */
