#include <stdexcept>
#include <cstdarg>

#include "qagen-dicom.h"
#include "qagen-memory.h"
#include "qagen-error.h"

#define INVALID_BEAM_NUMBER -1


EXTERN_C
int qagen_rtplan_load(struct qagen_rtplan *rp, const wchar_t *filename)
{
    try {
        RPReader reader(filename, rp);
        reader.read_tags();
        return 0;
    } catch (DCMReader::Exception &) {
        /* Error raised by DCMReader::Exception::Exception */
    } catch (std::bad_alloc &) {
        int errnum = ENOMEM;
        qagen_error_raise(QAGEN_ERR_SYSTEM, &errnum, L"std::bad_alloc thrown while reading DICOM");
    } catch (std::exception &) {
        qagen_error_raise(QAGEN_ERR_RUNTIME, L"Cannot read DICOM", L"Unknown std::exception thrown while reading DICOM");
    }
    qagen_rtplan_destroy(rp);
    return 1;
}


EXTERN_C
void qagen_rtplan_destroy(struct qagen_rtplan *rp)
{
    qagen_freezero(rp->beam);
}


EXTERN_C
int qagen_rtdose_load(struct qagen_rtdose *rd, const wchar_t *filename)
{
    try {
        RDReader reader(filename, rd);
        reader.read_tags();
        return 0;
    } catch (DCMReader::Exception &) {
        /* Error raised by DCMReader::Exception::Exception */
    } catch (std::bad_alloc &) {
        int errnum = ENOMEM;
        qagen_error_raise(QAGEN_ERR_SYSTEM, &errnum, L"std::bad_alloc thrown while reading DICOM");
    } catch (std::exception &) {
        qagen_error_raise(QAGEN_ERR_RUNTIME, L"Cannot read DICOM", L"Unknown std::exception thrown while reading DICOM");
    }
    qagen_rtdose_destroy(rd);
    return 1;
}


EXTERN_C
void qagen_rtdose_destroy(struct qagen_rtdose *rd)
{
    (void)rd;
}


EXTERN_C
bool qagen_rtdose_instance_match(const struct qagen_rtdose *rd,
                                 const struct qagen_rtplan *rp)
{
    return !std::strcmp(rd->sop_inst_ref_uid, rp->sop_inst_uid);
}


EXTERN_C
bool qagen_rtdose_isnumbered(const struct qagen_rtdose *rd)
{
    return rd->beamnum != INVALID_BEAM_NUMBER;
}


DCMReader::Exception::Exception(OFCondition stat, const wchar_t *msg)
{
    std::wstring str;
    size_t len;
    len = std::mbstowcs(NULL, stat.text(), 0);
    if (len != (size_t)-1) {
        str.resize(len + 1);
        std::mbstowcs(str.data(), stat.text(), str.size());
        qagen_error_raise(QAGEN_ERR_RUNTIME, str.c_str(), msg);
    } else {
        qagen_error_raise(QAGEN_ERR_RUNTIME, NULL, msg);
    }
}


void DCMReader::Exception::ofcheck(OFCondition stat, const wchar_t *restrict fmt, ...)
{
    wchar_t buf[256];
    std::va_list args;
    if (stat.bad()) {
        va_start(args, fmt);
        std::vswprintf(buf, BUFLEN(buf), fmt, args);
        va_end(args);
        throw Exception(stat, buf);
    }
}


DCMReader::DCMReader(const wchar_t *filename)
{
    OFCondition stat = m_dcfile.loadFile(OFFilename(filename));
    Exception::ofcheck(stat, L"Failed to load DICOM file");
    m_dset = m_dcfile.getDataset();
}


RPReader::RPReader(const wchar_t *filename, struct qagen_rtplan *rp):
    DCMReader(filename),
    m_rp(rp)
{
    m_rp->beam = nullptr;
}


void RPReader::read_sop_instance_uid(void)
{
    static const wchar_t *failmsg = L"Failed to get SOP instance UID from RTPlan";
    OFCondition stat;
    const char *res;
    stat = m_dset->findAndGetString(DCM_SOPInstanceUID, res);
    Exception::ofcheck(stat, failmsg);
    std::strcpy(m_rp->sop_inst_uid, res);
}


void RPReader::read_number_of_beams(void)
{
    static const wchar_t *failmsg = L"Failed to get number of beams from RTPlan";
    OFCondition stat;
    DcmItem *item;
    Sint32 res;
    stat = m_dset->findAndGetSequenceItem(DCM_FractionGroupSequence, item);
    Exception::ofcheck(stat, failmsg);
    stat = item->findAndGetSint32(DCM_NumberOfBeams, res);
    Exception::ofcheck(stat, failmsg);
    m_rp->nbeams = static_cast<std::uint32_t>(res);
}


void RPReader::alloc_beams(void)
{
    void *ptr = qagen_malloc(sizeof *m_rp->beam * m_rp->nbeams);
    if (ptr) {
        m_rp->beam = reinterpret_cast<struct qagen_rtplan::qagen_rtbeam *>(ptr);
    } else {
        throw std::bad_alloc();
    }
}


void RPReader::read_single_beam(DcmItem *seqitem, std::uint32_t i)
{
    static const wchar_t *failfmt = L"RTPlan is missing %s for beam %u";
    OFCondition stat;
    const char *str;
    Float64 dub;

    stat = seqitem->findAndGetString(DCM_TreatmentMachineName, str);
    Exception::ofcheck(stat, failfmt, L"TreatmentMachineName", i);
    std::mbstowcs(m_rp->beam[i].machine, str, BUFLEN(m_rp->beam[i].machine));

    stat = seqitem->findAndGetString(DCM_BeamName, str);
    Exception::ofcheck(stat, failfmt, L"BeamName", i);
    std::mbstowcs(m_rp->beam[i].name, str, BUFLEN(m_rp->beam[i].name));

    stat = seqitem->findAndGetString(DCM_BeamDescription, str);
    Exception::ofcheck(stat, failfmt, L"BeamDescription", i);
    std::mbstowcs(m_rp->beam[i].desc, str, BUFLEN(m_rp->beam[i].desc));

    stat = seqitem->findAndGetFloat64(DCM_FinalCumulativeMetersetWeight, dub);
    Exception::ofcheck(stat, failfmt, L"FinalCumulativeMetersetWeight", i);
    m_rp->beam[i].meterset = dub;

    stat = seqitem->findAndGetSequenceItem(DCM_RangeShifterSequence, seqitem);
    Exception::ofcheck(stat, failfmt, L"RangeShifterSequence", i);

    stat = seqitem->findAndGetString(DCM_RangeShifterID, str);
    Exception::ofcheck(stat, failfmt, L"RangeShifterID", i);
    m_rp->beam[i].rs_id = static_cast<std::int8_t>(std::atoi(str));
}


void RPReader::read_beams(void)
{
    static const wchar_t *failfmt = L"Failed to read beam %u from RTPlan";
    OFCondition stat;
    DcmItem *item;
    for (std::uint32_t i = 0; i < m_rp->nbeams; i++) {
        stat = m_dset->findAndGetSequenceItem(DCM_IonBeamSequence, item, i);
        Exception::ofcheck(stat, failfmt, i + 1);
        read_single_beam(item, i);
    }
}


void RPReader::read_tags(void)
{
    read_sop_instance_uid();
    read_number_of_beams();
    alloc_beams();
    read_beams();
}


void RDReader::read_referenced_sop_instance(DcmItem *refrpseq)
{
    static const wchar_t *failmsg = L"Failed to get ReferencedSOPInstanceUID from RTDose";
    OFCondition stat;
    const char *res;
    stat = refrpseq->findAndGetString(DCM_ReferencedSOPInstanceUID, res);
    Exception::ofcheck(stat, failmsg);
    std::strcpy(m_rd->sop_inst_ref_uid, res);
}


void RDReader::read_beamnum(void)
{
    static const wchar_t *failmsg = L"RTDose is missing required sequence item";
    OFCondition stat;
    DcmItem *item;
    Sint32 res;

    stat = m_dset->findAndGetSequenceItem(DCM_ReferencedRTPlanSequence, item);
    Exception::ofcheck(stat, failmsg);

    read_referenced_sop_instance(item);

    stat = item->findAndGetSequenceItem(DCM_ReferencedFractionGroupSequence, item);
    if (stat.bad()) {
        m_rd->beamnum = INVALID_BEAM_NUMBER;
        return;
    }
    stat = item->findAndGetSequenceItem(DCM_ReferencedBeamSequence, item);
    Exception::ofcheck(stat, failmsg);
    stat = item->findAndGetSint32(DCM_ReferencedBeamNumber, res);
    Exception::ofcheck(stat, failmsg);
    m_rd->beamnum = static_cast<int32_t>(res);
}


RDReader::RDReader(const wchar_t *filename, struct qagen_rtdose *rd):
    DCMReader(filename),
    m_rd(rd)
{
    
}


void RDReader::read_tags(void)
{
    read_beamnum();
}
