#pragma once
/** @file DICOM handling */
#ifndef QAGEN_DICOM_H
#define QAGEN_DICOM_H

#include <stdint.h>
#include "qagen-defs.h"

EXTERN_C_START


/** SOPInstanceUID          [NEW]
 *  FractionGroupSequence
 *  └─NumberOfBeams
 *  IonBeamSequence
 *  ├─TreatmentMachineName
 *  ├─BeamName
 *  ├─BeamDescription
 *  ├─RangeShifterSequence
 *  │ └─RangeShifterID
 *  └─FinalCumulativeMetersetWeight
 */
struct qagen_rtplan {
    char sop_inst_uid[65];  /* SOP Instance UID of this RTPlan */

    uint32_t nbeams;
    struct qagen_rtbeam {
        wchar_t machine[3];
        uint8_t rs_id;
        wchar_t name[6];
        wchar_t desc[22];
        double  meterset;
    } *beam;
};


/** @brief Loads data from the file at @p filename into the struct @p rp
 *  @param rp
 *      RTPlan struct
 *  @param filename
 *      Path to DICOM RTPlan file
 *  @returns Nonzero on error
 */
int qagen_rtplan_load(struct qagen_rtplan *rp, const wchar_t *filename);


/** @brief Frees the beam vector 
 *  @param rp
 *      RTPlan struct
 */
void qagen_rtplan_destroy(struct qagen_rtplan *rp);


/** ReferencedRTPlanSequence
 *  ├─ReferencedSOPInstanceUID          [NEW]
 *  └─ReferencedFractionGroupSequence   [OPTIONAL]
 *    └─ReferencedBeamSequence
 *      └─ReferencedBeamNumber
 */
struct qagen_rtdose {
    char sop_inst_ref_uid[65];  /* ReferencedSOPInstanceUID */

    int32_t beamnum;    /* Negative if this beam does not contain a number */
};


/** @brief Loads data from the file at @p filename into the struct @p rd
 *  @param rd
 *      RTDose struct
 *  @param filename
 *      Path to DICOM RTDose file
 *  @returns Nonzero on error
 */
int qagen_rtdose_load(struct qagen_rtdose *rd, const wchar_t *filename);


/** @brief Currently a nop
 *  @param rd
 *      RTDose struct
 */
void qagen_rtdose_destroy(struct qagen_rtdose *rd);


/** @brief Determines if the ReferencedSOPInstanceUID of @p rd matches the
 *      SOPInstanceUID of @p rp
 *  @param rd
 *      RTDose struct
 *  @param rp
 *      RTPlan struct
 *  @returns true if @p rd belongs to @p rp's instance
 */
bool qagen_rtdose_instance_match(const struct qagen_rtdose *rd,
                                 const struct qagen_rtplan *rp);


/** @brief Determines if this file has a valid beam number */
bool qagen_rtdose_isnumbered(const struct qagen_rtdose *rd);


EXTERN_C_END


#if defined(__cplusplus) || __cplusplus
#   include <dcmtk/dcmdata/dcdatset.h>
#   include <dcmtk/dcmdata/dcdeftag.h>
#   include <dcmtk/dcmdata/dcfilefo.h>


/** @class Generic DICOM reader containing common code and declarations */
class DCMReader {
public:

    /** @struct Raises an application error when constructed */
    struct Exception {

        /** @brief Raises a runtime error with context stat.text() and message
         *      @p msg
         *  @param stat
         *      DCMTK status object
         *  @param msg
         *      Error message
         */
        Exception(OFCondition stat, const wchar_t *msg);

        /** @brief Checks @p stat, and if it is a failure status, throws an
         *      instance of this class
         *  @param stat
         *      DCMTK status object
         *  @param fmt
         *      Format string used for raising a runtime error
         */
        static void ofcheck(OFCondition stat, const wchar_t *restrict fmt, ...);
    };

protected:
    DcmFileFormat m_dcfile;
    DcmDataset   *m_dset;

public:

    /** @brief Creates a new DICOM reader to read the DICOM file at @p filename
     *  @param filename
     *      Path to DICOM file
     */
    explicit DCMReader(const wchar_t *filename);

    /** @note This is only here to enforce the interface */
    virtual void read_tags(void) = 0;
};


/** @class Reads the relevant information from DICOM files using the RTPlan
 *      media storage SOP
 */
class RPReader: public DCMReader {
    struct qagen_rtplan *m_rp;

    void read_sop_instance_uid(void);

    void read_number_of_beams(void);
    void alloc_beams(void);

    /** @brief Reads the necessary data for a single beam, indexed at @p i
     *  @param seqitem
     *      The DCMTK DICOM item object containing beam information
     *  @param i
     *      The (zero-indexed) index of this beam
     */
    void read_single_beam(DcmItem *seqitem, std::uint32_t i);
    void read_beams(void);

public:
    RPReader(const wchar_t *filename, struct qagen_rtplan *rp);

    virtual void read_tags(void) override;
};


/** @class Reads the relevant information from DICOM RTDose files */
class RDReader: public DCMReader {
    struct qagen_rtdose *m_rd;

    /** @brief Reads directly from the ReferencedRTPlanSequence item
     *      @p refrpseq
     *  @param refrpseq
     *      ReferencedRTPlanSequence item #0
     */
    void read_referenced_sop_instance(DcmItem *refrpseq);

    void read_beamnum(void);

public:
    RDReader(const wchar_t *filename, struct qagen_rtdose *rd);

    virtual void read_tags(void) override;
};


#endif /* CXX_ONLY */


#endif /* QAGEN_DICOM_H */
