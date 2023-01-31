#include <cstdarg>
#include "qagen-metaio.h"
#include "qagen-error.h"
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcvrdt.h>


EXTERN_C
int qagen_metaio_convert(const wchar_t *restrict mhd,
                         const wchar_t *restrict dst,
                         const wchar_t *restrict tmplt)
{
    static const wchar_t *failmsg = L"Failed to convert MHD to DICOM";
    try {
        MHDConverter cvtr(mhd, tmplt);
        cvtr.convert(dst);
        return 0;
    } catch (MHDConverter::Exception &) {
        /* Already raised */
    } catch (std::bad_alloc &) {
        int err = ENOMEM;
        qagen_error_raise(QAGEN_ERR_SYSTEM, &err, failmsg);
    } catch (std::exception &) {
        qagen_error_raise(QAGEN_ERR_RUNTIME, failmsg, L"Caught unknown polymorphic std::exception");
    }
    return 1;
}


const wchar_t *MHDConverter::m_failmsg = L"Cannot convert MHD file";


MHDConverter::Exception::Exception(const wchar_t *restrict msg, const wchar_t *restrict fmt, ...)
{
    wchar_t buf[128];
    std::va_list args;
    va_start(args, fmt);
    std::vswprintf(buf, BUFLEN(buf), fmt, args);
    va_end(args);
    qagen_error_raise(QAGEN_ERR_RUNTIME, msg, buf);
}


MHDConverter::Exception::Exception(const wchar_t *restrict fmt, ...)
{
    wchar_t buf[128];
    std::va_list args;
    va_start(args, fmt);
    std::vswprintf(buf, BUFLEN(buf), fmt, args);
    va_end(args);
    qagen_error_raise(QAGEN_ERR_RUNTIME, m_failmsg, buf);
}


MHDConverter::Exception::Exception(OFCondition stat, const wchar_t *restrict fmt, ...)
{
    wchar_t buf1[128], buf2[128];
    std::va_list args;
    std::mbstowcs(buf1, stat.text(), BUFLEN(buf1));
    va_start(args, fmt);
    std::vswprintf(buf2, BUFLEN(buf2), fmt, args);
    va_end(args);
    qagen_error_raise(QAGEN_ERR_RUNTIME, buf1, buf2);
}


MHDConverter::Exception::Exception(int errnum, const wchar_t *restrict fmt, ...)
{
    wchar_t buf[128];
    std::va_list args;
    va_start(args, fmt);
    std::vswprintf(buf, BUFLEN(buf), fmt, args);
    va_end(args);
    qagen_error_raise(QAGEN_ERR_SYSTEM, &errnum, buf);
}


void MHDConverter::Exception::ofcheck(OFCondition stat, const wchar_t *msg)
{
    if (stat.bad()) {
        throw Exception(stat, msg);
    }
}


void MHDConverter::load_template(const wchar_t *fname)
{
    OFCondition stat;
    stat = m_dcfile.loadFile(OFFilename(fname));
    Exception::ofcheck(stat, L"Cannot load RD template");
}


void MHDConverter::load_mhd(const wchar_t *fname)
{
    char buf[512];
    if (std::wcstombs(buf, fname, BUFLEN(buf)) == -1) {
        throw Exception(EILSEQ, L"Cannot convert MHD input path to UTF-8");
    }
    if (!m_mhd.Read(buf)) {
        throw Exception(L"Cannot read MHD: %s", fname);
    }
    if (m_mhd.NDims() != 3) {
        throw Exception(L"MHD has invalid dimensionality %d", m_mhd.NDims());
    }
    if (m_mhd.ElementType() != MET_FLOAT) {  
        throw Exception(L"Invalid MHD encoding: Expected MET_FLOAT, found %S", MET_ValueTypeName[m_mhd.ElementType()]);
    }
}


void MHDConverter::convert_time(DcmDataset *dset)
{
    OFCondition stat;
    OFDateTime now;
    OFString date, time;
    if (!now.setCurrentDateTime()) {
        return;
    }
    if (!now.getDate().getISOFormattedDate(date, false)
     || !now.getTime().getISOFormattedTime(time, true, true, false, false)) {
        return;
    }
    /* Just ignore any errors, do we really care about these tags? */
    dset->putAndInsertString(DCM_CreationDate, date.c_str());
    dset->putAndInsertString(DCM_ContentDate, date.c_str());
    dset->putAndInsertString(DCM_CreationTime, time.c_str());
    dset->putAndInsertString(DCM_ContentTime, time.c_str());
}


void MHDConverter::convert_uid(DcmDataset *dset)
{
    static const wchar_t *failmsg = L"MHD conversion: Failed to set output UIDs";
    char uid[64] = "2.16.840.999.999";  /* US prefix, with just 9's */
    OFCondition stat;
    stat = dset->putAndInsertString(DCM_SOPInstanceUID, uid);
    Exception::ofcheck(stat, failmsg);
    stat = dset->putAndInsertString(DCM_SeriesInstanceUID, uid);
    Exception::ofcheck(stat, failmsg);
}


void MHDConverter::convert_strings(DcmDataset *dset)
{
    static const wchar_t *failmsg = L"MHD conversion: Failed to insert string tag";
    static const char *str[] = {
        "JHU Sibley Proton Center",
        "MCSquare Dose Calculation",
        "MCSquare Dose",
        "PHYSICAL",
        "GY"
    };
    static const DcmTagKey key[] = {
        DCM_Manufacturer,
        DCM_ManufacturerModelName,
        DCM_SeriesDescription,
        DCM_DoseType,
        DCM_DoseUnits
    };
    OFCondition stat;
    static_assert(BUFLEN(str) == BUFLEN(key));
    for (std::size_t i = 0; i < BUFLEN(str); i++) {
        stat = dset->putAndInsertString(key[i], str[i]);
        Exception::ofcheck(stat, failmsg);
    }
}


void MHDConverter::convert_grid_frame_offset_vector(DcmDataset *dset)
{
    static const wchar_t *failmsg = L"MHD conversion: Failed to insert GridFrameOffsetVector";
    char buf[128];
    double pos = m_mhd.ElementSpacing(2);
    OFCondition stat;
    std::string gfov = "0";
    for (int i = 0; i < m_mhd.DimSize(2); i++) {
        std::sprintf(buf, "\\%g", pos);
        gfov += buf;
        pos += m_mhd.ElementSpacing(2);
    }
    stat = dset->putAndInsertString(DCM_GridFrameOffsetVector, gfov.c_str());
    Exception::ofcheck(stat, failmsg);
}


void MHDConverter::convert_geometry(DcmDataset *dset)
{
    static const wchar_t *failmsg = L"MHD conversion: Failed to insert geometric information";
    char buf[128];
    const double *const spacing = m_mhd.ElementSpacing();
    const double *const origin = m_mhd.Origin();
    const int *const dim = m_mhd.DimSize();
    OFCondition stat;

    std::snprintf(buf, BUFLEN(buf), "%g\\%g", spacing[0], spacing[1]);
    stat = dset->putAndInsertString(DCM_PixelSpacing, buf);
    Exception::ofcheck(stat, failmsg);

    std::snprintf(buf, BUFLEN(buf), "%g", spacing[2]);
    stat = dset->putAndInsertString(DCM_SliceThickness, buf);
    Exception::ofcheck(stat, failmsg);

    stat = dset->putAndInsertUint16(DCM_Columns, dim[0]);
    Exception::ofcheck(stat, failmsg);
    stat = dset->putAndInsertUint16(DCM_Rows, dim[1]);
    Exception::ofcheck(stat, failmsg);
    std::snprintf(buf, BUFLEN(buf), "%d", dim[2]);
    stat = dset->putAndInsertString(DCM_NumberOfFrames, buf);
    Exception::ofcheck(stat, failmsg);

    std::snprintf(buf, BUFLEN(buf), "%g\\%g\\%g", origin[0], origin[1], origin[2]);
    stat = dset->putAndInsertString(DCM_ImagePositionPatient, buf);
    Exception::ofcheck(stat, failmsg);

    convert_grid_frame_offset_vector(dset);
}

/* Microsoft pls, stop */
#undef max


template <class DataT>
DataT datamax(const DataT *d0, const DataT *const d1)
{
    DataT res = (DataT)0;
    for (; d0 < d1; d0++) {
        res = std::max(res, *d0);
    }
    return res;
}


template <class DataT, class PixelT>
void MHDConverter::convert_pixels(DcmDataset *dset)
{
    const size_t framelen = (std::size_t)m_mhd.DimSize(0) * m_mhd.DimSize(1);
    const size_t n = framelen * m_mhd.DimSize(2);
    const DataT *dptr = reinterpret_cast<DataT *>(m_mhd.ElementData());
    const DataT dosegridscal = datamax(dptr, dptr + n) / (DataT)std::numeric_limits<PixelT>::max();
    PixelT *dest, *dstptr;
    OFCondition stat;
    std::string buf = std::to_string(dosegridscal);
    stat = dset->putAndInsertString(DCM_DoseGridScaling, buf.c_str());
    Exception::ofcheck(stat, L"MHD conversion: Failed to update DoseGridScaling");
    dest = dstptr = new PixelT[n];
    /* Write each frame backwards */
    dptr += framelen;
    for (int k = 0; k < m_mhd.DimSize(2); k++) {
        std::generate(dstptr, dstptr + framelen, [&dptr, dosegridscal](){
            return static_cast<PixelT>(*--dptr / dosegridscal);
        });
        dptr += 2 * framelen;
        dstptr += framelen;
    }
    stat = dset->putAndInsertUint8Array(DCM_PixelData, reinterpret_cast<Uint8 *>(dest), static_cast<unsigned long>(n * sizeof *dest));
    delete[] dest;
    Exception::ofcheck(stat, L"Failed to set the pixel data");
}


MHDConverter::MHDConverter(const wchar_t *restrict mhd, const wchar_t *restrict tmplt)
{
    load_template(tmplt);
    load_mhd(mhd);
}


void MHDConverter::convert(const wchar_t *dst)
{
    OFCondition stat;
    DcmDataset *dset = m_dcfile.getDataset();
    convert_time(dset);
    convert_uid(dset);
    convert_strings(dset);
    convert_geometry(dset);
    convert_pixels<float, uint16_t>(dset);
    stat = m_dcfile.saveFile(OFFilename(dst));
    Exception::ofcheck(stat, L"Failed to save converted DICOM file");
}
