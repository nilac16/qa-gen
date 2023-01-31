#pragma once

#ifndef QAGEN_PATIENT_JSON_H
#define QAGEN_PATIENT_JSON_H

#include "qagen-defs.h"
#include "qagen-patient.h"


/** @brief Writes patient information to a JSON file
 *  @param pt
 *      Patient context
 *  @param filename
 *      Path to write JSON to
 *  @returns Nonzero on error
 *  @note This function always returns zero. If the JSON cannot be created due
 *      to error, this should not stop the operation
 *  @warning Again, the patient context must have a valid RP file. This is not
 *      checked
 */
int qagen_json_write(const struct qagen_patient *pt, const wchar_t *filename);


#endif /* QAGEN_PATIENT_JSON_H */
