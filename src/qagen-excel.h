#pragma once

#ifndef QAGEN_EXCEL_H
#define QAGEN_EXCEL_H

#include "qagen-defs.h"
#include "qagen-patient.h"


/** @brief Writes a report form to @p filename using the information in @p pt
 *  @param pt
 *      Patient context
 *  @param filename
 *      Destination filename
 *  @returns Nonzero on error
 */
int qagen_excel_write(const struct qagen_patient *pt, const wchar_t *filename);


#endif /* QAGEN_EXCEL_H */
