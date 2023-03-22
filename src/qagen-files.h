#pragma once
/** @file This file handles the... files that we are concerned with
 * 
 *  @todo The behavior of this API needs to be hardened. I anticipate that
 *      malformed DICOM files will cause the entire algorithm to terminate with
 *      an error state. We should simply ignore files that cannot be loaded,
 *      but log a warning that a file was skipped.
 *      
 *      I think this should also account for system errors. This file perhaps
 *      should not be raising application errors at all, or they should be
 *      cleared after calling
 */
#ifndef QAGEN_FILE_H
#define QAGEN_FILE_H

#include <stdint.h>
#include "qagen-defs.h"
#include "qagen-dicom.h"
#include "qagen-path.h"


typedef enum qagen_file_type {
    QAGEN_FILE_DCM_RP,
    QAGEN_FILE_DCM_RD,
    QAGEN_FILE_DCM_DOSEBEAM,
    QAGEN_FILE_MHD_DOSEBEAM,

    QAGEN_FILE_OTHER    /* If this is set, the union has invalid data. The
                        owning scope will need to know what this list contains */
} qagen_file_t;


struct qagen_file {
    struct qagen_file *next;
    qagen_file_t type;
    union {
        struct qagen_rtplan rp;
        struct qagen_rtdose rd;
    } data;
    wchar_t name[MAX_PATH]; /* Filename */
    wchar_t path[];         /* Fully-qualified path */
};


/** @brief Creates a list of all files in directory @p dir matching pattern
 *      string @p pattern, assuming that they are of type @p type
 *  @param type
 *      Expected type of each file
 *  @param dir
 *      Base directory to search
 *  @param pattern
 *      Pattern to search files
 *  @returns A pointer to the head of a list containing relevant file info.
 *      Note that NULL will be returned if no files are found, but no error
 *      will be raised in such a case, so the caller must check it to
 *      distinguish
 */
struct qagen_file *qagen_file_enumerate(qagen_file_t   type,
                                        const PATH    *dir,
                                        const wchar_t *pattern);


/** @brief Filters RTDose files in list @p rd
 *  @details Each file must both contain a valid beam number, and it must
 *      reference the SOPInstanceUID of @p rp
 *  @param rd
 *      Pointer to pointer to head of RTDose list
 *  @param rp
 *      RTPlan file containing the chosen SOPInstanceUID
 *  @warning This function does *not* validate its arguments whatsoever (i.e.
 *      it engages in dangerous union accesses, assuming that you passed it the
 *      correct lists)
 */
void qagen_file_filter_rd(struct qagen_file **rd, const struct qagen_file *rp);


/** @brief Frees the file list
 *  @param head
 *      Head of the list
 */
void qagen_file_list_free(struct qagen_file *head);


/** @brief Finds the length of @p head
 *  @param head
 *      Head of the file list
 *  @returns The number of nodes in the list at @p head
 *  @note @p head may be NULL, and will result in zero length
 */
unsigned qagen_file_list_len(const struct qagen_file *head);


/** @brief Extracts the node at index @p idx from the list and frees the rest
 *  @param head
 *      The head of the list
 *  @param idx
 *      The zero-indexed position of the node to be extracted
 *  @returns The extracted node
 *  @warning This function is not bounds-checked in any way. Make sure your
 *      code is correct before invoking this
 */
struct qagen_file *qagen_file_list_extract(struct qagen_file *head, int idx);


/** @brief Allocates and writes strings that describe each of the fields.
 *  @param head
 *      Head of the RTPlan list
 *  @param[out] str
 *      Pointer to location where the pointer to an array of pointers to the
 *      strings will be written (just pass this a pointer to a wchar_t **, and
 *      it will take care of the rest)
 *  @returns The number of strings, or zero on error
 *  @note If this function succeeds, then the strings must be freed with a call
 *      to qagen_file_beam_strings_free
 */
int qagen_file_beam_strings(const struct qagen_file *head, wchar_t ***str);


/** @brief Frees beam strings previously created by a call to
 *      qagen_file_beam_strings
 *  @param nstr
 *      Number of strings
 *  @param str
 *      Pointer to array of string pointers
 */
void qagen_file_beam_strings_free(int nstr, wchar_t **str);


/** @brief Computes the total size of every file in the list
 *  @param head
 *      Head of file list
 *  @returns The accumulated sum total of each file size in @p head
 *  @note MHD files are very small, and this does not fetch the size of their
 *      data files. Use the RD template to approximate their size
 */
ULONGLONG qagen_file_list_totalsize(const struct qagen_file *head);


#endif /* QAGEN_FILE_H */
