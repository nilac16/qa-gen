#pragma once

#ifndef QAGEN_IMG2DCM_H
#define QAGEN_IMG2DCM_H

#include "qagen-defs.h"

EXTERN_C_START


/** @brief Convert @p img to a DICOM file, and save it to @p dcm
 *  @param img
 *      Input image path. This image must be readable by ITK
 *  @param dcm
 *      Output DICOM path
 *  @param tmplt
 *      Template DICOM file to be used
 */
int qagen_img2dcm_convert(const wchar_t *restrict img,
                          const wchar_t *restrict dcm,
                          const wchar_t *restrict tmplt);


EXTERN_C_END

#if defined(__cplusplus) && __cplusplus

#include <array>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <itkImage.h>


class ITKConverter {
public:
    /* Raise an application error if this is thrown. No need to derive from an
    exception class, because I know exactly where this comes from and what it
    does */
    class Exception {

    public:
        /* General ITK exception */
        Exception(const itk::ExceptionObject &e,
                  const wchar_t     *restrict ctx,
                  ...)
            noexcept;

        /* General DCMTK error */
        Exception(OFCondition stat, const wchar_t *ctx, ...) noexcept;

        /* DICOM attribute insertion failure */
        Exception(const DcmTagKey &key, OFCondition stat) noexcept;
    };

private:
    using pixel_t = Uint16;
    using data_t = float;
    using image_t = itk::Image<data_t, 3>;
    image_t::Pointer m_img;
    DcmFileFormat    m_dcfile;

    std::array<unsigned, 3> m_dim;
    std::array<double, 3> m_res, m_org;

    double m_dgs;


    /** These are marked noexcept in spite of the dereference operator's
     *  disposition to throw if the target pointer is NULL. Do not misuse these
     *  or you will go straight to SIGSEGV land
     */
    image_t &image() noexcept { return *m_img; }
    const image_t &image() const noexcept { return *m_img; }

    image_t::Pointer &image_ptr() noexcept { return m_img; }

    DcmFileFormat &dcmfile() noexcept { return m_dcfile; }
    const DcmFileFormat &dcmfile() const noexcept { return m_dcfile; }

    DcmDataset *dataset() noexcept { return dcmfile().getDataset(); }

    std::array<unsigned, 3> &dimensions() noexcept { return m_dim; }
    unsigned dimension(unsigned i) const noexcept { return m_dim[i]; }
    unsigned &dimension(unsigned i) noexcept { return m_dim[i]; }

    std::array<double, 3> &spacing() noexcept { return m_res; }
    double spacing(unsigned i) const noexcept { return m_res[i]; }
    double &spacing(unsigned i) noexcept { return m_res[i]; }

    std::array<double, 3> &origin() noexcept { return m_org; }
    double origin(unsigned i) const noexcept { return m_org[i]; }
    double &origin(unsigned i) noexcept { return m_org[i]; }

    double &dose_gridscaling() noexcept { return m_dgs; }
    double dose_gridscaling() const noexcept { return m_dgs; }


    template <class InsertT>
    void insert(const DcmTagKey &tag, size_t n, const InsertT val[]);

    template <class InsertT>
    void insert(const DcmTagKey &tag, InsertT val) { insert(tag, 1, &val); }

    void insert(const DcmTagKey &tag, const char *val);
    void insert(const DcmTagKey &tag, const DcmTagKey &val);

    void insert_pixels(const std::vector<pixel_t> &px);

    /** Catch ITK's polymorphic exception from this, I have no idea what
     *  exceptions ITK throws nor when...
     */
    void load_image(const wchar_t *path);

    /** DCMTK is much better about its exceptions, don't catch from this */
    void load_template(const wchar_t *path);

    void write_datetime() noexcept; /* Fails silently */
    void write_strings();
    void write_geometry();
    void write_scaling();

    void write_attributes();
    void write_pixels();

public:
    ITKConverter() noexcept;
    ITKConverter(const wchar_t *restrict img, const wchar_t *restrict tmplt);


    /** @brief Convert to a DICOM dataset in memory
     *  @param img
     *      Path to ITK-readable image file
     *  @param tmplt
     *      Path to DICOM RTDose file used as a template
     */
    void initialize(const wchar_t *restrict img, const wchar_t *restrict tmplt);


    /** @brief Flush the dataset to disk
     *  @param path
     *      Path to write the dataset to
     */
    void write(const wchar_t *path);
};


#endif /* __cplusplus */

#endif /* QAGEN_IMG2DCM_H */
