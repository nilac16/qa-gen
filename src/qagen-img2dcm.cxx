#include <cstdarg>
#include <string>
#include <itkImageFileReader.h>
#include <itkMinimumMaximumImageCalculator.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcvrdt.h>
#include "qagen-img2dcm.h"
#include "qagen-error.h"

using namespace std::literals;


EXTERN_C
int qagen_img2dcm_convert(const wchar_t *restrict img,
                          const wchar_t *restrict dcm,
                          const wchar_t *restrict tmplt)
{
    static const wchar_t *failmsg = L"Cannot convert image to DICOM file";
    ITKConverter cvt;
    int erno;

    try {
        cvt.initialize(img, tmplt);
        cvt.write(dcm);
        return 0;
    } catch (const ITKConverter::Exception &) {
        /* Already raised */
    } catch (const std::bad_alloc &) {
        erno = ENOMEM;
        qagen_error_raise(QAGEN_ERR_SYSTEM, &erno, failmsg);
    } catch (const std::exception &) {
        qagen_error_raise(QAGEN_ERR_RUNTIME, L"Caught unknown polymorphic std::exception", failmsg);
    }
    return 1;
}


ITKConverter::Exception::Exception(const itk::ExceptionObject &e,
                                   const wchar_t     *restrict ctx,
                                   ...)
    noexcept
{
    wchar_t buf[256];
    va_list args;

    va_start(args, ctx);
    vswprintf(buf, BUFLEN(buf), ctx, args);
    va_end(args);
    qagen_error_raise(QAGEN_ERR_RUNTIME, buf, L"%S", e.what());
}


ITKConverter::Exception::Exception(OFCondition stat, const wchar_t *ctx, ...)
    noexcept
{
    wchar_t buf[256];
    va_list args;

    va_start(args, ctx);
    vswprintf(buf, BUFLEN(buf), ctx, args);
    va_end(args);
    qagen_error_raise(QAGEN_ERR_RUNTIME, ctx, L"%S", stat.text());
}


ITKConverter::Exception::Exception(const DcmTagKey &key, OFCondition stat)
    noexcept
{
    wchar_t buf[256];

    swprintf(buf, BUFLEN(buf), L"Cannot insert element (%04x,%04x)",
                               key.getGroup(), key.getElement());
    qagen_error_raise(QAGEN_ERR_RUNTIME, buf, L"%S", stat.text());
}


template <class InsertT>
void ITKConverter::insert(const DcmTagKey &tag, size_t n, const InsertT val[])
{
    static const char *delim[] = { "", "\\" };
    std::stringstream ss;
    unsigned i, j = 0;
    OFCondition stat;

    for (i = 0; i < n; i++) {
        ss << delim[j] << val[i];
        j |= (unsigned)1;
    }
    stat = dataset()->putAndInsertString(tag, ss.str().c_str());
    if (stat.bad()) {
        throw Exception(tag, stat);
    }
}


/* Not sure yet if I need this specialization */
/* template <>
void ITKConverter::insert(const DcmTagKey  &tag,
                          size_t            n,
                          const char *const val[])
{

} */


void ITKConverter::insert(const DcmTagKey &tag, const char *val)
{
    OFCondition stat;

    stat = dataset()->putAndInsertString(tag, val);
    if (stat.bad()) {
        throw Exception(tag, stat);
    }
}


void ITKConverter::insert(const DcmTagKey &tag, const DcmTagKey &val)
{
    OFCondition stat;

    stat = dataset()->putAndInsertTagKey(tag, val);
    if (stat.bad()) {
        throw Exception(tag, stat);
    }
}


void ITKConverter::insert_pixels(const std::vector<pixel_t> &px)
{
    OFCondition stat;

    stat = dataset()->putAndInsertUint16Array(DCM_PixelData,
                                              px.data(),
                                              px.size());
    if (stat.bad()) {
        throw Exception(DCM_PixelData, stat);
    }
}


void ITKConverter::load_image(const wchar_t *path)
{
    using reader_t = itk::ImageFileReader<image_t>;
    reader_t::Pointer reader;   /* The default constructor isn't noexcept... */
    char buf[256];  /* We're just gonna do it this way */

    snprintf(buf, BUFLEN(buf), "%S", path);
    reader = reader_t::New();
    reader->SetFileName(buf);
    reader->Update();
    image_ptr() = reader->GetOutput();
}


void ITKConverter::load_template(const wchar_t *path)
{
    OFFilename fname(path);
    OFCondition stat;

    stat = dcmfile().loadFile(fname);
    if (stat.bad()) {
        throw Exception(stat, L"Cannot read template file");
    }
}


void ITKConverter::write_datetime()
    noexcept
/** I could be totally wrong, but I believe that DCMTK does *NOT* allow
 *  bad_allocs to slip out
 */
{
    OFString date, time;
    OFCondition stat;
    OFDateTime now;

    if (!now.setCurrentDateTime()) {
        return;
    }
    if (!now.getDate().getISOFormattedDate(date, false)
     || !now.getTime().getISOFormattedTime(time, true, true, false, false)) {
        return;
    }
    /* Don't use ITKConverter::insert; it throws and this is noexcept */
    dataset()->putAndInsertString(DCM_CreationDate, date.c_str());
    dataset()->putAndInsertString(DCM_ContentDate, date.c_str());
    dataset()->putAndInsertString(DCM_CreationTime, time.c_str());
    dataset()->putAndInsertString(DCM_ContentTime, time.c_str());
}


void ITKConverter::write_strings()
{
    using pair_t = std::pair<DcmTagKey, const char *>;
    static const char *uid_all = "2.16.840.999.999";
    const pair_t pairs[] = {
        { DCM_SOPInstanceUID,        uid_all                     },
        { DCM_SeriesInstanceUID,     uid_all                     },
        { DCM_Manufacturer,          "JHU Sibley Proton Center"  },
        { DCM_ManufacturerModelName, "MCSquare Dose Calculation" },
        { DCM_SeriesDescription,     "MCSquare Dose"             },
        { DCM_DoseType,              "PHYSICAL"                  },
        { DCM_DoseUnits,             "GY"                        }
    };

    for (const pair_t &pair: pairs) {
        insert(pair.first, pair.second);
    }
}


void ITKConverter::write_geometry()
/** None of these ITK calls are noexcept... */
{
    std::vector<float> gfov;
    double orient[9];
    unsigned i;

    for (i = 0; i < 3; i++) {
        dimension(i) = image().GetBufferedRegion().GetSize(i);
        spacing(i) = image().GetSpacing()[i];
        origin(i) = image().GetOrigin()[i];
        std::copy(&image().GetDirection()[i][0],
                  &image().GetDirection()[i][3],
                  &orient[3 * i]);
    }
    insert(DCM_Rows, dimension(1));
    insert(DCM_Columns, dimension(0));
    insert(DCM_NumberOfFrames, dimension(2));
    insert(DCM_PixelSpacing, 2, spacing().data());
    insert(DCM_SliceThickness, spacing(2));
    insert(DCM_ImageOrientationPatient, 6, orient);
    insert(DCM_ImagePositionPatient, 3, origin().data());
    for (i = 0; i < dimension(2); i++) {
        gfov.push_back((double)i * spacing(2));
    }
    insert(DCM_FrameIncrementPointer, DCM_GridFrameOffsetVector);
    insert(DCM_GridFrameOffsetVector, gfov.size(), gfov.data());
}


void ITKConverter::write_scaling()
{
    using calc_t = itk::MinimumMaximumImageCalculator<image_t>;
    calc_t::Pointer calc;
    data_t max;

    calc = calc_t::New();
    calc->SetImage(image_ptr());
    calc->ComputeMaximum();
    max = calc->GetMaximum();
    dose_gridscaling() = max / (data_t)std::numeric_limits<pixel_t>::max();
    insert(DCM_DoseGridScaling, dose_gridscaling());
}


void ITKConverter::write_attributes()
{
    write_datetime();
    write_strings();
    write_geometry();
    write_scaling();
}


void ITKConverter::write_pixels()
{
    using pxvector_t = std::vector<pixel_t>;
    pxvector_t pixels;
    pixel_t *ptr, *end;
    itk::Index<3> idx;
    OFCondition stat;
    size_t framelen;

    framelen = (size_t)dimension(0) * dimension(1);
    pixels.resize(framelen * dimension(2));
    end = pixels.data() + framelen;
    for (idx[2] = 0; idx[2] < dimension(2); idx[2]++) {
        ptr = end;
        for (idx[1] = 0; idx[1] < dimension(1); idx[1]++) {
            for (idx[0] = 0; idx[0] < dimension(0); idx[0]++) {
                *--ptr = image().GetPixel(idx) / dose_gridscaling();
            }
        }
        end += framelen;
    }
    insert_pixels(pixels);
}


ITKConverter::ITKConverter()
    noexcept
{

}


ITKConverter::ITKConverter(const wchar_t *restrict img,
                           const wchar_t *restrict tmplt)
{
    initialize(img, tmplt);
}


void ITKConverter::initialize(const wchar_t *restrict img,
                              const wchar_t *restrict tmplt)
{
    try {
        load_image(img);
        load_template(tmplt);
        write_attributes();
        write_pixels();
    } catch (const itk::ExceptionObject &e) {
        throw Exception(e, L"Cannot read input image"); /* how helpful */
    }
}


void ITKConverter::write(const wchar_t *path)
{
    OFFilename fname(path);
    OFCondition stat;

    stat = dcmfile().saveFile(fname);
    if (stat.bad()) {
        throw Exception(stat, L"Cannot save DICOM file to disk");
    }
}
