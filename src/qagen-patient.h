#pragma once
/** @file A patient context containing strings and files to be copied/converted
 */
#ifndef QAGEN_PATIENT_H
#define QAGEN_PATIENT_H

#include "qagen-defs.h"
#include "qagen-files.h"
#include "qagen-path.h"
 
#define BEAMSET_LIMIT 16    /* I believe this is imposed by Raystation itself */
#define FOLDER_LIMIT  22 + BEAMSET_LIMIT   /* LlFf_(x.xx,y.yy,z.zz)-<BEAMSET> */


enum {
    PT_TOK_MRN,
    PT_TOK_LNAME,
    PT_TOK_FNAME,
    PT_TOK_PLAN,
    PT_TOK_BEAMSET,
    PT_TOK_ISO,
    PT_N_TOKENS
};


struct qagen_patient {
    wchar_t *tokens[PT_N_TOKENS];   /* These tokens are created from the
                                    display name string, which itself is
                                    created/managed by the ShellItem */
    double iso[3];

    DWORD pt_idx;  /* The index of this patient, if the user selected multiple */
    DWORD pt_tot;  /* Total patients selected */

    wchar_t foldername[FOLDER_LIMIT];
    PATH   *basepath; /* Canonicalized path to the patient folder */

    struct qagen_file *rtplan;
    struct qagen_file *rtdose;
    struct qagen_file *dose_beam;   /* Could be DICOM or MHD files */
    struct qagen_file *rd_template;
};


/** @brief Parse the patient information out of the display name (cannibalizing
 *      it in the process)
 *  @param pt
 *      Patient context
 *  @param dpyname
 *      RS folder display name
 *  @param idx
 *      One-start index of this patient
 *  @param npatients
 *      Total patients selected
 *  @returns Nonzero on error
 */
int qagen_patient_init(struct qagen_patient *pt,
                       wchar_t              *dpyname,
                       DWORD                 idx,
                       DWORD                 npatients);


/** @brief Frees any memory held by the patient context
 *  @param pt
 *      Patient context
 */
void qagen_patient_cleanup(struct qagen_patient *pt);


/** @brief Returns the number of beams that we expect for this patient
 *  @param pt
 *      Patient context
 *  @returns The number of beams for this beamset
 *  @warning This function may *only* be invoked after an RP file has been
 *      found!
 */
uint32_t qagen_patient_num_beams(const struct qagen_patient *pt);


/** @brief Creates the QA folder for this patient. Also sets the base path
 *      member
 *  @details This creates the folder and its fixed contents: The MU directory,
 *      the JSON file, and the report form
 *  @param[in,out] pt
 *      Patient context
 *  @returns Nonzero on error
 *  @warning The patient struct must contain at least an RP file
 */
int qagen_patient_create_qa(struct qagen_patient *pt);


#endif /* QAGEN_PATIENT_H */
