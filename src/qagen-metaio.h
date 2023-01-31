#pragma once

#ifndef QAGEN_METAIO_H
#define QAGEN_METAIO_H

#include "qagen-defs.h"

EXTERN_C_START


/** @brief Convert the given @p mhd file to a DICOM file at @p dst
 *  @param mhd
 *      Path to MHD file
 *  @param dst
 *      Path to destination DICOM file
 *  @param tmplt
 *      Path to DICOM template file (I plan to make this optional)
 *  @returns Nonzero on error
 *  @note @p dst should be the *exact* path to the output file, including the
 *      desired file extension
 */
int qagen_metaio_convert(const wchar_t *restrict mhd,
                         const wchar_t *restrict dst,
                         const wchar_t *restrict tmplt);


EXTERN_C_END

#if defined(__cplusplus) || __cplusplus
#   include <dcmtk/dcmdata/dcdatset.h>
#   include <dcmtk/dcmdata/dcfilefo.h>
#   include <metaImage.h>


/** @class Loads MHD files and their data, and writes them out to DICOM RTDose
 *      files
 */
class MHDConverter {
public:
    /** @struct Exception class, raises an application error on construction */
    struct Exception {
        /** @brief Raise a runtime error with @p msg and @p fmt provided */
        Exception(const wchar_t *restrict msg, const wchar_t *restrict fmt, ...);

        /** @brief Raise a runtime error with message m_failmsg and context
         *      @p fmt
         */
        Exception(const wchar_t *restrict fmt, ...);

        /** @brief Raise a runtime error with context @p fmt and library message
         *      provided by @p stat
         */
        Exception(OFCondition stat, const wchar_t *restrict fmt, ...);

        /** @brief Raise a system error with errno @p errnum */
        Exception(int errnum, const wchar_t *restrict fmt, ...);

        /** @brief If stat.bad() == true, throws an instance of this class with
         *      @p msg
         */
        static void ofcheck(OFCondition stat, const wchar_t *msg);
    };

private:
    DcmFileFormat m_dcfile;
    MetaImage     m_mhd;

    static const wchar_t *m_failmsg;

    void load_template(const wchar_t *fname);
    void load_mhd(const wchar_t *fname);

    void convert_time(DcmDataset *dset);
    void convert_uid(DcmDataset *dset);
    void convert_strings(DcmDataset *dset);

    void convert_grid_frame_offset_vector(DcmDataset *dset);
    void convert_geometry(DcmDataset *dset);

    /* This template is complete. *Do* note that more of this class will need
    to change in order to accomodate different raw data/output pixel formats.
    Ultimately, detection and handling of arbitrary formats cannot be done with
    a template class/function */
    template <class DataT, class PixelT>
    void convert_pixels(DcmDataset *dset);

public:
    MHDConverter(const wchar_t *restrict mhd, const wchar_t *restrict tmplt);

    void convert(const wchar_t *dst);
};


#endif /* CXX_ONLY */

#endif /* QAGEN_METAIO_H */
