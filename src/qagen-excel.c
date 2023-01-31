/** This file is rapidly becoming the same disaster as the plotting code in 
 *  the shift app
 * 
 *  Hardcoding state machines is kind of difficult
 */
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <stdbool.h>

#include "qagen-excel.h"
#include "qagen-error.h"
#include "qagen-log.h"

#include <xlsxwriter.h>
#include <stdio.h>
#include <fcntl.h>

#define BK_TITLE    "QA Report"
#define BK_AUTHOR   "Chin-Cheng Chen; Calin Reamy"
#define BK_COMPANY  "SKCCC"
#define BK_COMMENT  "Created by QAGen"

#define FACILITY_TITLE   "The Johns Hopkins Proton Therapy Center"
#define MUCHECK_SUBTITLE "IMPT Patient 2nd MU Check"
#define IMPTQA_SUBTITLE  "Patient-Specific Quality Assurance"

#define MUSHEET "MU check"
#define QASHEET "IMPTQA"

#define FLD_PTNAME  "Patient name:"
#define FLD_PTMRN   "MRN:"
#define FLD_PTPLAN  "Plan name:"
#define FLD_BEAMSET "Beamset:"
#define FLD_RX      "Prescription:"
#define FLD_RX1     "Gy (RBE) in"
#define FLD_RX2     "fractions ("
#define FLD_RXFMLA  "=C10/E10/1.1"
#define FLD_RX3     "Gy/fx)"

/** Office 365 "helpfully" changed these to the new format (containing an @),
 *  which is not supported on Excel 2016 */
#define MU_CELL    "='"MUSHEET"'!%c%d"
#define MU_RANGE   "='"MUSHEET"'!%c%d:%c%d"

/** TODO: Validate this */
#define QA_ISO      "QA plan iso (cm):"
#define QA_RL       "R-L"
#define QA_SI       "S-I"
#define QA_AP       "P-A"

#define ISO_CELL    "=10*(%c%d-(25-$E$13))+1"

#define BEAM_NAME   "Beam name:"
#define BEAM_DESC   "Beam description:"
#define BEAM_MTST   "Meterset (MU):"

#define RES_DEPTH   "Depth (cm):"
#define RES_SLICE   "Slice depth (mm):"
#define RES_GP      "%GP:"

#define CCK_LABEL   "Data for change check"
#define CCK_HEADING "Rx value"
#define CCK_EXISTS  "Exists"

#define SIG_PERF    "Performed by:"
#define SIG_PHYS    "Physicist"
#define SIG_DATE    "Date:"
#define SIG_DTFMLA  "=TODAY()"
#define SIG_DTFMT   "mm/dd/yyyy"

#define HOPKINSBLUE 0x2F75B5
#define FIELDBG     0xF2F2F2

#define ALTBG       0xDDEBF7
#define BEAMFG      0x003270

#define SUCCESSGRN  0x10FE7C
#define SUCCESSALT  0x00EE6C
#define CCHKFG      0x00B050

#define ROW_SHORT   5
#define ROW_LONG    25

#define PTY 5

#include "res/skccclogo.h"


struct rpt_vec {
    lxw_row_t row;
    lxw_col_t col;
};

static void rpt_vec_cell(struct rpt_vec *vec, lxw_row_t r, lxw_col_t c)
{
    vec->row = r;
    vec->col = c;
}

static void rpt_vec_range(struct rpt_vec vec[], lxw_row_t r1, lxw_col_t c1, lxw_row_t r2, lxw_col_t c2)
{
    rpt_vec_cell(vec + 0, r1, c1);
    rpt_vec_cell(vec + 1, r2, c2);
}

static void rpt_vec_fmt_range(char dst[], const char *fmt, const struct rpt_vec range[])
{
    sprintf(dst, fmt, range[0].col + 'A', range[0].row + 1,
                      range[1].col + 'A', range[1].row + 1);
}

static void rpt_vec_fmt_cell(char dst[], const char *fmt, const struct rpt_vec *vec)
{
    sprintf(dst, fmt, vec->col + 'A', vec->row + 1);
}


/** All shared formats go here
 *  Pointers to formats used only once can go on the stack
 */
struct report {
    lxw_workbook *wb;
    lxw_worksheet *mu, *qa;

    int nbeams;

    struct rpt_vec isoy;

    struct {
        /* Field labels (lptlbl contains the left border) */
        lxw_format *ptlbl, *lptlbl;

        /* Blank cell on the left needs a border */
        lxw_format *lblank;

        /* Small Rx labels */
        lxw_format *smlbl;

        /* Fields have an underline and are colored differently */
        lxw_format *fldlbl;

        /* Reduce precision on MU/fx calculation */
        lxw_format *mufx;

        struct {
            /* Border formats applied to boundary cells */
            lxw_format *lo, *hi, *lt;
        } border;

        struct {
            struct rpt_vec name[2], mrn[2], plan[2], bmset[2], dose, nfx;
        } loc;
    } pt;

    struct {
        /* Labels on the left (each has a distinct border) */
        lxw_format *flbl1, *flbl2, *flbl3;

        /* Alternating colors for each column (lbls[0:2] for even cols, [3:5] for odd) */
        lxw_format *lbls[6];

        /* Border applied to the right (applied to the next column as a left border) */
        lxw_format *lbord;

        /* The row index containing the field names (in the MU sheet) */
        lxw_row_t rstart;
    } flds;

    struct {
        /* blbl2 is not used by the MU sheet */
        lxw_format *blbl1, *blbl2, *blbl3;

        /* These alternate in a single row: lbls1 is for the depth row, lbls2 for slice depth, lbls3 for %GP */
        lxw_format *lbls1[2], *lbls2[2], *lbls3[2];
        /* These are for the rightmost cells in the table, containing the right border */
        lxw_format *rlbls1[2], *rlbls2[2], *rlbls3[2];

        /* %GP conditional formats:
         *  Red if <90 or >100
         *  Green otherwise
         */
        lxw_conditional_format *pctgp;
    } beams;

    struct {
        /* lord have mercy */
        lxw_format *cap;
        lxw_format *lt, *rt;
        lxw_format *hi, *lo;
        lxw_format *tl, *tr;
        lxw_format *bl, *br;
    } fig;

    struct {
        lxw_format *lbl;
        lxw_format *phys;
        lxw_format *date;
    } sig;
};


#define FMTARG(func, arg) func, _Generic((arg), double: (size_t)3, default: sizeof arg), arg

/** @brief Constructs a format from a sequence of function pointers/args
 *  @warning This is very dangerous, and must be invoked with the utmost care
 *  @details The variadic arguments are a sequence of {F, S, D}, where F is a
 *      pointer to a function taking (lxw_format *, D), S is the size of D in
 *      bytes, and D is the data itself. The FMTARG macro expands to one of
 *      these triples for you, so you are encouraged to use it in all cases.
 *      The sequence is terminated by a single NULL pointer where a function
 *      pointer would be expected.
 *  @note Passing string literals to this function requires passing a pointer
 *      to their static storage. If you place a string literal in the FMTARG
 *      macro, Microsoft "helpfully" (doesn't happen with GCC) expands it to
 *      the size of the STRING and not the POINTER to it. Pass string literals
 *      like this: FMTARG(func, &(*(<literal>)))
 *  @param wb
 *      LXW workbook used to allocate the format
 *  @returns An LXW format object with the specified variadic attributes
 */
static lxw_format *qagen_excel_format_create(lxw_workbook *wb, ...)
{
    typedef void (*fmtf8)(lxw_format *, uint8_t);   // These two may not
    typedef void (*fmtf16)(lxw_format *, uint16_t); // actually be needed
    typedef void (*fmtf32)(lxw_format *, uint32_t); // Default promotion is
    typedef void (*fmtf64)(lxw_format *, uint64_t); // to 32 bits
    typedef void (*fmtfmm)(lxw_format *, double);
    lxw_format *res = workbook_add_format(wb);
    void *fmtfunc;
    va_list args;
    va_start(args, wb);
    do {
        fmtfunc = va_arg(args, void *);
        if (fmtfunc) {
            size_t width = va_arg(args, size_t);
            switch (width) {
            case 1:
                ((fmtf8)fmtfunc)(res, va_arg(args, uint8_t));
                break;
            case 2:
                ((fmtf16)fmtfunc)(res, va_arg(args, uint16_t));
                break;
            case 4:
                ((fmtf32)fmtfunc)(res, va_arg(args, uint32_t));
                break;
            case 8:
                ((fmtf64)fmtfunc)(res, va_arg(args, uint64_t));
                break;
            case 3:
                ((fmtfmm)fmtfunc)(res, va_arg(args, double));
                break;
            default:
                {
                    wchar_t msg[128];
                    swprintf(msg, BUFLEN(msg), L"Illegal width %zu encountered in Excel formatting function", width);
                    qagen_log_printf(QAGEN_LOG_ERROR, L"Unexpected width %zu!", width);
                    MessageBox(NULL, msg, L"Fatal", MB_ICONERROR);
                    abort();
                }
            }
        }
    } while (fmtfunc);
    va_end(args);
    return res;
}


static void qagen_excel_draw_header_logo(lxw_worksheet *sheet)
{
    lxw_image_options opt = {
        .x_offset = 10,
        .y_offset = 5,
        .decorative = 1
    };
    lxw_error e = worksheet_insert_image_buffer_opt(sheet, 0, 0, skccc,
                                                    sizeof skccc / sizeof *skccc,
                                                    &opt);
    if (e) {
        qagen_log_printf(QAGEN_LOG_ERROR, L"Failed to insert image: %S", lxw_strerror(e));
    }
}


/** Draws a header on the first 9 columns of the document (spans the entire 
 *  default width). The left section always contains the SKCCC logo. This 
 *  is always drawn at the origin. Spans 4 rows
 *  \param app
 *      Application state
 *  \param wb
 *      Current workbook
 *  \param sheet
 *      Worksheet to draw the header to
 *  \param bg
 *      Background color of the central portion of the header
 *  \param fg
 *      Foreground (text) color of the central header
 *  \param brdr
 *      Color of the header's external border. The central section is always 
 *      bounded by two gray borders
 *  \param title
 *      Upper string to be written to the central header section
 *  \param sub
 *      Lower string (subtitle) written one line below the title
 */
static void qagen_excel_draw_header(lxw_workbook  *wb,
                                    lxw_worksheet *sheet,
                                    lxw_color_t    bg,
                                    lxw_color_t    fg,
                                    lxw_color_t    brdr,
                                    const char    *title,
                                    const char    *sub)
{
    lxw_format *lformat = qagen_excel_format_create(wb,
                            FMTARG(format_set_border, LXW_BORDER_MEDIUM),
                            FMTARG(format_set_border_color, brdr),
                            FMTARG(format_set_right_color, LXW_COLOR_GRAY),
                            NULL);
    lxw_format *rformat = qagen_excel_format_create(wb,
                            FMTARG(format_set_border, LXW_BORDER_MEDIUM),
                            FMTARG(format_set_border_color, brdr),
                            FMTARG(format_set_left_color, LXW_COLOR_GRAY),
                            NULL);
    lxw_format *hdrtop  = qagen_excel_format_create(wb,
                            FMTARG(format_set_border_color, brdr),
                            FMTARG(format_set_top, LXW_BORDER_MEDIUM),
                            FMTARG(format_set_bottom, LXW_BORDER_NONE),
                            FMTARG(format_set_bg_color, bg),
                            NULL);
    lxw_format *hdrtxt  = qagen_excel_format_create(wb,
                            FMTARG(format_set_border, LXW_BORDER_NONE),
                            FMTARG(format_set_bg_color, bg),
                            FMTARG(format_set_font_color, fg),
                            FMTARG(format_set_bold, 0),
                            FMTARG(format_set_align, LXW_ALIGN_CENTER),
                            FMTARG(format_set_align, LXW_ALIGN_VERTICAL_TOP),
                            FMTARG(format_set_font_size, 13.0),
                            NULL);
    lxw_format *hdrbot  = qagen_excel_format_create(wb,
                            FMTARG(format_set_bg_color, bg),
                            FMTARG(format_set_border_color, brdr),
                            FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
                            FMTARG(format_set_top, LXW_BORDER_NONE),
                            NULL);
    /* Resize rows */
    worksheet_set_row(sheet, 1, ROW_LONG, NULL);
    worksheet_set_row(sheet, 2, ROW_LONG, NULL);
    /* Construct header */
    worksheet_merge_range(sheet, 0, 0, 3, 1, NULL, lformat);
    worksheet_merge_range(sheet, 0, 8, 3, 8, NULL, rformat);
    worksheet_merge_range(sheet, 0, 2, 0, 7, NULL, hdrtop);
    worksheet_merge_range(sheet, 1, 2, 1, 7, title, hdrtxt);
    worksheet_merge_range(sheet, 2, 2, 2, 7, sub, hdrtxt);
    worksheet_merge_range(sheet, 3, 2, 3, 7, NULL, hdrbot);
    qagen_excel_draw_header_logo(sheet);
}


#define PTINFOWIDTH  8
#define PTINFOHEIGHT 5


static void qagen_excel_draw_pt_border(struct report *rpt,
                                       lxw_worksheet *sheet,
                                       lxw_row_t      row)
{
    int i;
    /* Upper border */
    for (i = 0; i < PTINFOWIDTH; i++) {
        worksheet_write_blank(sheet, row - 1, i, rpt->pt.border.lo);
    }
    /* Right border */
    for (i = 0; i < PTINFOHEIGHT; i++) {
        worksheet_write_blank(sheet, row + i, PTINFOWIDTH, rpt->pt.border.lt);
    }
    /* Lower border */
    for (i = 0; i < PTINFOWIDTH; i++) {
        worksheet_write_blank(sheet, row + PTINFOHEIGHT, i, rpt->pt.border.hi);
    }
}


static void qagen_excel_draw_pt_info_copied(struct report *rpt,
                                            lxw_worksheet *sheet,
                                            lxw_row_t      row,
                                            lxw_format    *flbl)
{
    char formula[128];
    worksheet_merge_range(sheet, row + 0, 2, row + 0, 4, "", flbl);
    rpt_vec_fmt_range(formula, MU_RANGE, rpt->pt.loc.name);
    worksheet_write_formula(sheet, row + 0, 2, formula, flbl);

    worksheet_merge_range (sheet, row + 0, 6, row + 0, 7, "", flbl);
    rpt_vec_fmt_range(formula, MU_RANGE, rpt->pt.loc.mrn);
    worksheet_write_formula(sheet, row + 0, 6, formula, flbl);

    worksheet_merge_range(sheet, row + 2, 2, row + 2, 4, "", flbl);
    rpt_vec_fmt_range(formula, MU_RANGE, rpt->pt.loc.plan);
    worksheet_write_formula(sheet, row + 2, 2, formula, flbl);

    worksheet_merge_range(sheet, row + 2, 6, row + 2, 7, "", flbl);
    rpt_vec_fmt_range(formula, MU_RANGE, rpt->pt.loc.bmset);
    worksheet_write_formula(sheet, row + 2, 6, formula, flbl);

    /* This cell will not draw conditional formatting */
    rpt_vec_fmt_cell(formula, MU_CELL, &rpt->pt.loc.dose);
    worksheet_write_formula(sheet, row + 4, 2, formula, flbl);
    rpt_vec_fmt_cell(formula, MU_CELL, &rpt->pt.loc.nfx);
    worksheet_write_formula(sheet, row + 4, 4, formula, flbl);
}


static lxw_conditional_format *qagen_excel_format_ptscript(lxw_workbook *wb)
{
    static lxw_conditional_format condf;    /* Does this actually need persistent storage???? */
    memset(&condf, 0, sizeof condf);
    condf.type   = LXW_CONDITIONAL_TYPE_BLANKS;
    condf.format = qagen_excel_format_create(wb,
                    FMTARG(format_set_bg_color, LXW_COLOR_RED),
                    NULL);
    return &condf;
}


static void qagen_excel_draw_pt_info_direct(const struct qagen_patient   *pt,
                                            struct report          *rpt,
                                            lxw_worksheet          *sheet,
                                            lxw_row_t               row,
                                            lxw_format             *flbl)
{
    char utf8buf[128];
    lxw_conditional_format *rxlbl = qagen_excel_format_ptscript(rpt->wb);
    snprintf(utf8buf, sizeof utf8buf, "%S, %S", pt->tokens[PT_TOK_LNAME], pt->tokens[PT_TOK_FNAME]);
    rpt_vec_range(rpt->pt.loc.name, row + 0, 2, row + 0, 4);
    worksheet_merge_range(sheet, row + 0, 2, row + 0, 4, utf8buf, flbl);

    wcstombs(utf8buf, pt->tokens[PT_TOK_MRN], sizeof utf8buf);
    rpt_vec_range(rpt->pt.loc.mrn, row + 0, 6, row + 0, 7);
    worksheet_merge_range(sheet, row + 0, 6, row + 0, 7, utf8buf, flbl);

    wcstombs(utf8buf, pt->tokens[PT_TOK_PLAN], sizeof utf8buf);
    rpt_vec_range(rpt->pt.loc.plan, row + 2, 2, row + 2, 4);
    worksheet_merge_range(sheet, row + 2, 2, row + 2, 4, utf8buf, flbl);

    wcstombs(utf8buf, pt->tokens[PT_TOK_BEAMSET], sizeof utf8buf);
    rpt_vec_range(rpt->pt.loc.bmset, row + 2, 6, row + 2, 7);
    worksheet_merge_range(sheet, row + 2, 6, row + 2, 7, utf8buf, flbl);

    rpt_vec_cell(&rpt->pt.loc.dose, row + 4, 2);
    worksheet_write_blank(sheet, row + 4, 2, flbl);
    worksheet_conditional_format_cell(sheet, row + 4, 2, rxlbl);

    rpt_vec_cell(&rpt->pt.loc.nfx, row + 4, 4);
    worksheet_write_blank(sheet, row + 4, 4, flbl);
    worksheet_conditional_format_cell(sheet, row + 4, 4, rxlbl);
}


/** Draws the patient info table onto the worksheet @p sheet. Spans 5 rows
 *  \param app
 *      Application state
 *  \param pt
 *      Patient structure
 *  \param rpt
 *      Report structure containing the current workbook and shared formats
 *  \param sheet
 *      Worksheet to draw the table to
 *  \param rstart
 *      Index of first row to draw the table in. The column is implicitly
 *      column zero
 *  \param scpy
 *      Specifies whether the patient information will be written to the 
 *      table. If true, it will be copied from the sheet labeled "MU check," 
 *      otherwise, it will be written directly
 */
static void qagen_excel_draw_pt_info(const struct qagen_patient *pt,
                                     struct report              *rpt,
                                     lxw_worksheet              *sheet,
                                     lxw_row_t                   rstart,
                                     bool                        scpy)
{
    /* Initialize rows */
    worksheet_set_row(sheet, rstart + 0, ROW_LONG, NULL);
    worksheet_set_row(sheet, rstart + 1, ROW_SHORT, NULL);
    worksheet_set_row(sheet, rstart + 2, ROW_LONG, NULL);
    worksheet_set_row(sheet, rstart + 3, ROW_SHORT, NULL);
    worksheet_set_row(sheet, rstart + 4, ROW_LONG, NULL);
    /* Construct segment */
    worksheet_merge_range(sheet, rstart + 0, 0, rstart + 0, 1, FLD_PTNAME, rpt->pt.lptlbl);
    worksheet_write_string(sheet, rstart + 0, 5, FLD_PTMRN, rpt->pt.ptlbl);
    worksheet_write_blank(sheet, rstart + 1, 0, rpt->pt.lblank);
    worksheet_merge_range(sheet, rstart + 2, 0, rstart + 2, 1, FLD_PTPLAN, rpt->pt.lptlbl);
    worksheet_write_string(sheet, rstart + 2, 5, FLD_BEAMSET, rpt->pt.ptlbl);
    worksheet_write_blank(sheet, rstart + 3, 0, rpt->pt.lblank);
    worksheet_merge_range (sheet, rstart + 4, 0, rstart + 4, 1, FLD_RX, rpt->pt.lptlbl);
    worksheet_write_string(sheet, rstart + 4, 3, FLD_RX1, rpt->pt.smlbl);
    worksheet_write_string(sheet, rstart + 4, 5, FLD_RX2, rpt->pt.smlbl);
    worksheet_write_formula(sheet, rstart + 4, 6, FLD_RXFMLA, rpt->pt.mufx);
    worksheet_write_string(sheet, rstart + 4, 7, FLD_RX3, rpt->pt.smlbl);
    if (scpy) {
        qagen_excel_draw_pt_info_copied(rpt, sheet, rstart, rpt->pt.fldlbl);
    } else {
        qagen_excel_draw_pt_info_direct(pt, rpt, sheet, rstart, rpt->pt.fldlbl);
    }
    qagen_excel_draw_pt_border(rpt, sheet, rstart);
}


static void qagen_excel_draw_field_col(const struct qagen_rtbeam *beam,
                                       lxw_worksheet             *sheet,
                                       lxw_row_t                  row,
                                       lxw_col_t                  col,
                                       lxw_format                *fmt[],
                                       bool                       scpy)
{
    char utf8buf[128];
    if (scpy) {
        sprintf(utf8buf, MU_CELL, col + 'A', row - 2);
        worksheet_write_formula(sheet, row + 0, col, utf8buf, fmt[0]);
        sprintf(utf8buf, MU_CELL, col + 'A', row - 1);
        worksheet_write_formula(sheet, row + 1, col, utf8buf, fmt[1]);
        sprintf(utf8buf, MU_CELL, col + 'A', row - 0);
        worksheet_write_formula(sheet, row + 2, col, utf8buf, fmt[2]);
    } else {
        wcstombs(utf8buf, beam->name, sizeof utf8buf);
        worksheet_write_string(sheet, row + 0, col, utf8buf, fmt[0]);
        wcstombs(utf8buf, beam->desc, sizeof utf8buf);
        worksheet_write_string(sheet, row + 1, col, utf8buf, fmt[1]);
        worksheet_write_number(sheet, row + 2, col, beam->meterset, fmt[2]);
    }
}


/** Draws the beam table for the specified patient @p pt. Spans 3 rows
 *  \param app
 *      Application state
 *  \param pt
 *      Patient structure
 *  \param rpt
 *      Report structure containing the current workbook and shared formats
 *  \param sheet
 *      Worksheet to draw the table to
 *  \param rstart
 *      Index of first row to draw the table in. The column is implicitly
 *      column zero
 *  \param scpy
 *      Specifies whether the patient information will be written to the 
 *      table. If true, it will be copied from the sheet labeled "MU check," 
 *      otherwise, it will be written directly
 */
static void qagen_excel_draw_field_info(const struct qagen_patient *pt,
                                        struct report              *rpt,
                                        lxw_worksheet              *sheet,
                                        lxw_row_t                   rstart,
                                        bool                        scpy)
{
    lxw_format **lbl[2] = { &rpt->flds.lbls[0], &rpt->flds.lbls[3] };
    int i;
    worksheet_set_row(sheet, rstart + 0, ROW_LONG, NULL);
    worksheet_set_row(sheet, rstart + 1, ROW_LONG, NULL);
    worksheet_set_row(sheet, rstart + 2, ROW_LONG, NULL);
    worksheet_merge_range(sheet, rstart + 0, 0, rstart + 0, 1, BEAM_NAME, rpt->flds.flbl1);
    worksheet_merge_range(sheet, rstart + 1, 0, rstart + 1, 1, BEAM_DESC, rpt->flds.flbl2);
    worksheet_merge_range(sheet, rstart + 2, 0, rstart + 2, 1, BEAM_MTST, rpt->flds.flbl3);
    for (i = 0; i < rpt->nbeams; i++) {
        qagen_excel_draw_field_col(&pt->rtplan->data.rp.beam[i], sheet,
                                   rstart, i + 2, lbl[i % 2], scpy);
    }
    worksheet_write_blank(sheet, rstart + 0, i + 2, rpt->flds.lbord);
    worksheet_write_blank(sheet, rstart + 1, i + 2, rpt->flds.lbord);
    worksheet_write_blank(sheet, rstart + 2, i + 2, rpt->flds.lbord);
}


static void qagen_excel_draw_depth_row(struct report *rpt,
                                       lxw_worksheet *sheet,
                                       lxw_row_t      row)
{
    int i;
    worksheet_merge_range(sheet, row, 0, row, 1, RES_DEPTH, rpt->beams.blbl1);
    for (i = 0; i < rpt->nbeams - 1; i++) {
        worksheet_write_blank(sheet, row, i + 2, rpt->beams.lbls1[i % 2]);
    }
    worksheet_write_blank(sheet, row, i + 2, rpt->beams.rlbls1[i % 2]);
}


static void qagen_excel_draw_pctgp_row(struct report *rpt,
                                       lxw_worksheet *sheet,
                                       lxw_row_t      row)
{
    int i;
    worksheet_merge_range(sheet, row, 0, row, 1, RES_GP, rpt->beams.blbl3);
    for (i = 0; i < rpt->nbeams - 1; i++) {
        worksheet_write_blank(sheet, row, i + 2, rpt->beams.lbls3[i % 2]);
        worksheet_conditional_format_cell(sheet, row, i + 2, rpt->beams.pctgp);
    }
    worksheet_write_blank(sheet, row, i + 2, rpt->beams.rlbls3[i % 2]);
    worksheet_conditional_format_cell(sheet, row, i + 2, rpt->beams.pctgp);
}


/** Draws a MU check result table row for a single depth. Spans 2 rows, but 
 *  modifies the row directly above (shortens it)
 */
static void qagen_excel_draw_mu_row(struct report *rpt,
                                    lxw_worksheet *sheet,
                                    lxw_row_t      rstart)
{
    worksheet_set_row(sheet, rstart - 1, ROW_SHORT, NULL);
    worksheet_set_row(sheet, rstart + 0, ROW_LONG, NULL);
    worksheet_set_row(sheet, rstart + 1, ROW_LONG, NULL);
    qagen_excel_draw_depth_row(rpt, sheet, rstart + 0);
    qagen_excel_draw_pctgp_row(rpt, sheet, rstart + 1);
}


/** Draws the MU check result table. Spans 9 rows, and modifies the row above 
 *  its starting index (shortens it)
 */
static void qagen_excel_draw_mu_table(struct report *rpt,
                                      lxw_worksheet *sheet,
                                      lxw_row_t      rstart)
{
    
    qagen_excel_draw_mu_row(rpt, sheet, rstart + 0);
    qagen_excel_draw_mu_row(rpt, sheet, rstart + 3);
    qagen_excel_draw_mu_row(rpt, sheet, rstart + 6);
}


/** Draws the siggy. One row */
static void qagen_excel_draw_signature(struct report *rpt,
                                       lxw_worksheet *sheet,
                                       lxw_row_t      row)
{
    worksheet_merge_range(sheet, row, 0, row, 1, SIG_PERF, rpt->sig.lbl);
    worksheet_merge_range(sheet, row, 2, row, 3, SIG_PHYS, rpt->sig.phys);
    worksheet_write_string(sheet, row, 5, SIG_DATE, rpt->sig.lbl);
    worksheet_merge_range(sheet, row, 6, row, 7, "", rpt->sig.date);
    worksheet_write_formula(sheet, row, 6, SIG_DTFMLA, rpt->sig.date);
}


/** Draws the isocenter table at the requested row index. Spans 2 rows
 *  \param app
 *      Application state
 *  \param pt
 *      Patient structure
 *  \param wb
 *      Current workbook
 *  \param sheet
 *      Worksheet to draw the table into
 *  \param rstart
 *      Row index to the beginning of the table
 */
static void qagen_excel_draw_iso_info(const struct qagen_patient *pt,
                                      lxw_workbook               *wb,
                                      lxw_worksheet              *sheet,
                                      lxw_row_t                   rstart)
{
    worksheet_set_row(sheet, rstart, 20, NULL);
    worksheet_set_row(sheet, rstart + 1, 20, NULL);
    worksheet_merge_range(sheet, rstart + 1, 0, rstart + 1, 1, QA_ISO,
        qagen_excel_format_create(wb,
            FMTARG(format_set_top, LXW_BORDER_MEDIUM),
            FMTARG(format_set_left, LXW_BORDER_MEDIUM),
            FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
            FMTARG(format_set_align, LXW_ALIGN_CENTER),
            FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
            NULL));
    worksheet_write_string(sheet, rstart, 2, QA_RL,
        qagen_excel_format_create(wb,
            FMTARG(format_set_top, LXW_BORDER_MEDIUM),
            FMTARG(format_set_left, LXW_BORDER_MEDIUM),
            FMTARG(format_set_align, LXW_ALIGN_CENTER),
            FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
            NULL));
    worksheet_write_string(sheet, rstart, 3, QA_SI,
        qagen_excel_format_create(wb,
            FMTARG(format_set_top, LXW_BORDER_MEDIUM),
            FMTARG(format_set_align, LXW_ALIGN_CENTER),
            FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
            NULL));
    worksheet_write_string(sheet, rstart, 4, QA_AP,
        qagen_excel_format_create(wb,
            FMTARG(format_set_top, LXW_BORDER_MEDIUM),
            FMTARG(format_set_right, LXW_BORDER_MEDIUM),
            FMTARG(format_set_align, LXW_ALIGN_CENTER),
            FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
            NULL));
    {
        lxw_format *shared = qagen_excel_format_create(wb,
            FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
            FMTARG(format_set_bg_color, FIELDBG),
            FMTARG(format_set_align, LXW_ALIGN_CENTER),
            FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
            FMTARG(format_set_num_format, &(*("0"))),
            NULL);
        lxw_format *notshared = qagen_excel_format_create(wb,
            FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
            FMTARG(format_set_right, LXW_BORDER_MEDIUM),
            FMTARG(format_set_bg_color, FIELDBG),
            FMTARG(format_set_align, LXW_ALIGN_CENTER),
            FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
            FMTARG(format_set_num_format, &(*("0"))),
            NULL);
        worksheet_write_number(sheet, rstart + 1, 2, pt->iso[0], shared);
        worksheet_write_number(sheet, rstart + 1, 3, pt->iso[1], shared);
        worksheet_write_number(sheet, rstart + 1, 4, pt->iso[2], notshared);
    }
}


static void qagen_excel_draw_slice_row(struct report *rpt,
                                       lxw_worksheet *sheet,
                                       lxw_row_t      row)
{
    char formula[128];
    int i;
    worksheet_merge_range(sheet, row, 0, row, 1, RES_SLICE, rpt->beams.blbl2);
    for (i = 0; i < rpt->nbeams - 1; i++) {
        sprintf(formula, ISO_CELL, i + 2 + 'A', row);
        worksheet_write_formula(sheet, row, i + 2, formula, rpt->beams.lbls2[i % 2]);
    }
    sprintf(formula, ISO_CELL, i + 2 + 'A', row);
    worksheet_write_formula(sheet, row, i + 2, formula, rpt->beams.rlbls2[i % 2]);
}


static void qagen_excel_space_result_table(lxw_worksheet *sheet,
                                           lxw_row_t      row)
{
    worksheet_set_row(sheet, row - 1, ROW_SHORT, NULL);
    worksheet_set_row(sheet, row + 0, ROW_LONG, NULL);
    worksheet_set_row(sheet, row + 1, ROW_LONG, NULL);
    worksheet_set_row(sheet, row + 2, ROW_LONG, NULL);
}


static void qagen_excel_draw_result_table(struct report *rpt,
                                          lxw_worksheet *sheet,
                                          lxw_row_t      rstart)
{
    qagen_excel_space_result_table(sheet, rstart);
    qagen_excel_draw_depth_row(rpt, sheet, rstart + 0);
    qagen_excel_draw_slice_row(rpt, sheet, rstart + 1);
    qagen_excel_draw_pctgp_row(rpt, sheet, rstart + 2);
    qagen_excel_space_result_table(sheet, rstart + 4);
    qagen_excel_space_result_table(sheet, rstart + 8);
}


static void qagen_excel_draw_change_check(struct report *rpt,
                                          lxw_worksheet *sheet,
                                          lxw_row_t      rstart)
{
    lxw_format *shared = qagen_excel_format_create(rpt->wb,
                            FMTARG(format_set_font_color, CCHKFG),
                            FMTARG(format_set_border_color, CCHKFG),
                            FMTARG(format_set_align, LXW_ALIGN_CENTER),
                            FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
                            NULL);
    int i;
    worksheet_merge_range(sheet, rstart + 1, 0, rstart + 1, 1, CCK_LABEL,
        qagen_excel_format_create(rpt->wb,
            FMTARG(format_set_font_color, CCHKFG),
            FMTARG(format_set_border_color, CCHKFG),
            FMTARG(format_set_font_size, 9.0),
            FMTARG(format_set_align, LXW_ALIGN_RIGHT),
            FMTARG(format_set_top, LXW_BORDER_MEDIUM),
            FMTARG(format_set_left, LXW_BORDER_MEDIUM),
            FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
            NULL));
    worksheet_merge_range(sheet, rstart, 2, rstart, rpt->nbeams + 1, CCK_HEADING,
        qagen_excel_format_create(rpt->wb,
            FMTARG(format_set_font_color, CCHKFG),
            FMTARG(format_set_border_color, CCHKFG),
            FMTARG(format_set_align, LXW_ALIGN_CENTER),
            FMTARG(format_set_left, LXW_BORDER_MEDIUM),
            FMTARG(format_set_top, LXW_BORDER_MEDIUM),
            FMTARG(format_set_right, LXW_BORDER_MEDIUM),
            NULL));
    for (i = 0; i < rpt->nbeams - 1; i++) {
        worksheet_write_string(sheet, rstart + 1, i + 2, CCK_EXISTS, shared);
    }
    worksheet_write_string(sheet, rstart + 1, i + 2, CCK_EXISTS,
        qagen_excel_format_create(rpt->wb,
            FMTARG(format_set_font_color, CCHKFG),
            FMTARG(format_set_border_color, CCHKFG),
            FMTARG(format_set_align, LXW_ALIGN_CENTER),
            FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
            FMTARG(format_set_right, LXW_BORDER_MEDIUM),
            NULL));
}


static void qagen_excel_draw_figure(struct report  *rpt, 
                                    lxw_worksheet  *sheet,
                                    struct rpt_vec *org,
                                    struct rpt_vec *size,
                                    struct rpt_vec *ref)
{
    char formula[128];
    lxw_row_t i;
    lxw_col_t j;
    rpt_vec_fmt_cell(formula, "=%c%d", ref);
    worksheet_write_formula(sheet, org->row - 1, org->col, formula, rpt->fig.cap);

    worksheet_write_blank(sheet, org->row, org->col, rpt->fig.tl);
    for (j = 1; j < size->col - 1; j++)
        worksheet_write_blank(sheet, org->row, org->col + j, rpt->fig.hi);

    worksheet_write_blank(sheet, org->row, org->col + size->col - 1, rpt->fig.tr);
    for (i = 1; i < size->row - 1; i++)
        worksheet_write_blank(sheet, org->row + i, org->col + size->col - 1, rpt->fig.rt);

    worksheet_write_blank(sheet, org->row + size->row - 1, org->col + size->col - 1, rpt->fig.br);
    for (j--; j > 0; j--)
        worksheet_write_blank(sheet, org->row + size->row - 1, org->col + j, rpt->fig.lo);

    worksheet_write_blank(sheet, org->row + size->row - 1, org->col, rpt->fig.bl);
    for (i--; i > 0; i--)
        worksheet_write_blank(sheet, org->row + i, org->col, rpt->fig.lt);
}


static void qagen_excel_draw_figures(struct report *rpt,
                                     lxw_worksheet *sheet,
                                     int            nbeams)
{
    struct rpt_vec org, width, ref;
    if (nbeams > 2) {
        rpt_vec_cell(&org, 39, 0);
        rpt_vec_cell(&width, 13, 4);
        rpt_vec_cell(&ref, 14, 2);
        qagen_excel_draw_figure(rpt, sheet, &org, &width, &ref);
        rpt_vec_cell(&org, 39, 5);
        rpt_vec_cell(&width, 13, 4);
        rpt_vec_cell(&ref, 14, 3);
        qagen_excel_draw_figure(rpt, sheet, &org, &width, &ref);
        switch (nbeams) {
        case 6:
            rpt_vec_cell(&org, 69, 5);
            rpt_vec_cell(&width, 13, 4);
            rpt_vec_cell(&ref, 14, 7);
            qagen_excel_draw_figure(rpt, sheet, &org, &width, &ref);
        case 5:
            rpt_vec_cell(&org, 69, 0);
            rpt_vec_cell(&width, 13, 4);
            rpt_vec_cell(&ref, 14, 6);
            qagen_excel_draw_figure(rpt, sheet, &org, &width, &ref);
        case 4:
            rpt_vec_cell(&org, 54, 5);
            rpt_vec_cell(&width, 13, 4);
            rpt_vec_cell(&ref, 14, 5);
            qagen_excel_draw_figure(rpt, sheet, &org, &width, &ref);
        case 3:
            rpt_vec_cell(&org, 54, 0);
            rpt_vec_cell(&width, 13, 4);
            rpt_vec_cell(&ref, 14, 4);
            qagen_excel_draw_figure(rpt, sheet, &org, &width, &ref);
        }
    } else {
        switch (nbeams) {
        case 2:
            rpt_vec_cell(&org, 24, 5);
            rpt_vec_cell(&width, 10, 4);
            rpt_vec_cell(&ref, 14, 3);
            qagen_excel_draw_figure(rpt, sheet, &org, &width, &ref);
        case 1:
            rpt_vec_cell(&org, 14, 5);
            rpt_vec_cell(&width, 9, 4);
            rpt_vec_cell(&ref, 14, 2);
            qagen_excel_draw_figure(rpt, sheet, &org, &width, &ref);
        }
    }
    if (nbeams < 3) {
        qagen_excel_draw_signature(rpt, rpt->qa, 35);
    } else if (nbeams < 5) {
        qagen_excel_draw_signature(rpt, rpt->qa, 69);
    } else if (nbeams < 7) {
        qagen_excel_draw_signature(rpt, rpt->qa, 83);
    }
}


static void qagen_excel_mu_sheet(const struct qagen_patient *pt,
                                 struct report              *rpt)
{
    qagen_excel_draw_header(rpt->wb, rpt->mu, HOPKINSBLUE, LXW_COLOR_WHITE,
                            BEAMFG, FACILITY_TITLE, MUCHECK_SUBTITLE);
    qagen_excel_draw_pt_info(pt, rpt, rpt->mu, 5, false);
    rpt->flds.rstart = 11;
    qagen_excel_draw_field_info(pt, rpt, rpt->mu, rpt->flds.rstart, false);
    qagen_excel_draw_mu_table(rpt, rpt->mu, 15);
    qagen_excel_draw_signature(rpt, rpt->mu, 28);
}


static void qagen_excel_qa_sheet(const struct qagen_patient *pt,
                                 struct report              *rpt)
{
    qagen_excel_draw_header(rpt->wb, rpt->qa, LXW_COLOR_WHITE, HOPKINSBLUE,
                            LXW_COLOR_BLACK, FACILITY_TITLE, IMPTQA_SUBTITLE);
    qagen_excel_draw_pt_info(pt, rpt, rpt->qa, 5, true);
    qagen_excel_draw_iso_info(pt, rpt->wb, rpt->qa, 11);
    qagen_excel_draw_field_info(pt, rpt, rpt->qa, 14, true);
    qagen_excel_draw_result_table(rpt, rpt->qa, 18);
    qagen_excel_draw_change_check(rpt, rpt->qa, 32);
    qagen_excel_draw_figures(rpt, rpt->qa, rpt->nbeams);
}


static void qagen_excel_report_init_ptfmt(struct report *rpt)
{
    rpt->pt.ptlbl  = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_align, LXW_ALIGN_RIGHT),
                        NULL);
    rpt->pt.lptlbl = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_align, LXW_ALIGN_RIGHT),
                        FMTARG(format_set_left, LXW_BORDER_MEDIUM),
                        NULL);
    rpt->pt.lblank = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_left, LXW_BORDER_MEDIUM),
                        NULL);
    rpt->pt.smlbl  = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_font_size, 10.0),
                        FMTARG(format_set_align, LXW_ALIGN_LEFT),
                        NULL);
    rpt->pt.fldlbl = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_bg_color, FIELDBG),
                        FMTARG(format_set_bottom, LXW_BORDER_THIN),
                        NULL);
    /* rpt->pt.mufx = workbook_add_format(rpt->wb);
    format_set_num_format(rpt->pt.mufx, "0.00"); */
    rpt->pt.mufx = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_align, LXW_ALIGN_CENTER),
                        FMTARG(format_set_num_format, &(*("0.00"))),
                        NULL);
    rpt->pt.border.lo = qagen_excel_format_create(rpt->wb, 
                            FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
                            NULL);
    rpt->pt.border.hi = qagen_excel_format_create(rpt->wb, 
                            FMTARG(format_set_top, LXW_BORDER_MEDIUM),
                            NULL);
    rpt->pt.border.lt = qagen_excel_format_create(rpt->wb, 
                            FMTARG(format_set_left, LXW_BORDER_MEDIUM),
                            NULL);
}


static lxw_format *qagen_excel_format_fldlbl(lxw_workbook *wb,
                                             lxw_color_t   brdr,
                                             lxw_color_t   txt)
{
    lxw_format *res = workbook_add_format(wb);
    format_set_border_color(res, brdr);
    format_set_left(res, LXW_BORDER_MEDIUM);
    format_set_align(res, LXW_ALIGN_RIGHT);
    format_set_align(res, LXW_ALIGN_VERTICAL_CENTER);
    format_set_font_color(res, txt);
    return res;
}


static void qagen_excel_field_format_base(lxw_workbook *wb, lxw_format *fmt[])
{
    fmt[0] = qagen_excel_format_create(wb,
                FMTARG(format_set_border_color, BEAMFG),
                FMTARG(format_set_top, LXW_BORDER_MEDIUM),
                FMTARG(format_set_align, LXW_ALIGN_CENTER),
                FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
                FMTARG(format_set_font_color, BEAMFG),
                NULL);
    fmt[1] = qagen_excel_format_create(wb,
                FMTARG(format_set_border_color, BEAMFG),
                FMTARG(format_set_align, LXW_ALIGN_CENTER),
                FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
                FMTARG(format_set_font_size, 8.0),
                NULL);
    fmt[2] = qagen_excel_format_create(wb,
                FMTARG(format_set_border_color, BEAMFG),
                FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
                FMTARG(format_set_align, LXW_ALIGN_CENTER),
                FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
                FMTARG(format_set_num_format, &(*("0.00"))),
                NULL);
}


static void qagen_excel_get_field_format(lxw_workbook *wb, lxw_format *fmt[])
{
    qagen_excel_field_format_base(wb, fmt + 0);
    qagen_excel_field_format_base(wb, fmt + 3);
    format_set_bg_color(fmt[0], ALTBG);
    format_set_bg_color(fmt[1], ALTBG);
    format_set_bg_color(fmt[2], ALTBG);
}


static void qagen_excel_report_init_fldfmt(struct report *rpt)
{
    rpt->flds.flbl1 = qagen_excel_format_fldlbl(rpt->wb, BEAMFG, BEAMFG);
    rpt->flds.flbl2 = qagen_excel_format_fldlbl(rpt->wb, BEAMFG, BEAMFG);
    rpt->flds.flbl3 = qagen_excel_format_fldlbl(rpt->wb, BEAMFG, BEAMFG);
    format_set_top(rpt->flds.flbl1, LXW_BORDER_MEDIUM);
    format_set_bottom(rpt->flds.flbl3, LXW_BORDER_MEDIUM);
    qagen_excel_get_field_format(rpt->wb, rpt->flds.lbls);
    rpt->flds.lbord = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_left, LXW_BORDER_MEDIUM),
                        FMTARG(format_set_border_color, BEAMFG),
                        NULL);
}


static void qagen_excel_get_beam_format(lxw_workbook *wb,
                                        lxw_format *fmt1[],
                                        lxw_format *fmt2[],
                                        int         rbord)
{
    fmt1[0] = qagen_excel_format_create(wb,
                    FMTARG(format_set_top, LXW_BORDER_MEDIUM),
                    FMTARG(format_set_right, rbord),
                    FMTARG(format_set_bg_color, ALTBG),
                    FMTARG(format_set_align, LXW_ALIGN_CENTER),
                    FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
                    NULL);
    fmt1[1] = qagen_excel_format_create(wb,
                    FMTARG(format_set_top, LXW_BORDER_MEDIUM),
                    FMTARG(format_set_right, rbord),
                    FMTARG(format_set_align, LXW_ALIGN_CENTER),
                    FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
                    NULL);
    fmt2[0] = qagen_excel_format_create(wb,
                    FMTARG(format_set_right, rbord),
                    FMTARG(format_set_bg_color, ALTBG),
                    FMTARG(format_set_align, LXW_ALIGN_CENTER),
                    FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
                    NULL);
    fmt2[1] = qagen_excel_format_create(wb,
                    FMTARG(format_set_right, rbord),
                    FMTARG(format_set_align, LXW_ALIGN_CENTER),
                    FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
                    NULL);
}


static lxw_conditional_format *qagen_excel_format_pctgp(lxw_workbook *wb)
{
    static lxw_conditional_format condf;
    memset(&condf, 0, sizeof condf);
    condf.type      = LXW_CONDITIONAL_TYPE_CELL;
    condf.criteria  = LXW_CONDITIONAL_CRITERIA_NOT_BETWEEN;
    condf.min_value = nextafter(0.9, 0.0);
    condf.max_value = nextafter(1.0, 2.0);
    condf.format    = qagen_excel_format_create(wb,
                        FMTARG(format_set_bg_color, LXW_COLOR_RED),
                        FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
                        FMTARG(format_set_align, LXW_ALIGN_CENTER),
                        FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
                        FMTARG(format_set_num_format, &(*("0.0%"))),
                        NULL);
    return &condf;
}


static void qagen_excel_report_init_beamfmt(struct report *rpt)
{
    rpt->beams.blbl1 = qagen_excel_format_fldlbl(rpt->wb, LXW_COLOR_BLACK, LXW_COLOR_BLACK);
    rpt->beams.blbl2 = qagen_excel_format_fldlbl(rpt->wb, LXW_COLOR_BLACK, LXW_COLOR_BLACK);
    rpt->beams.blbl3 = qagen_excel_format_fldlbl(rpt->wb, LXW_COLOR_BLACK, LXW_COLOR_BLACK);
    format_set_top(rpt->beams.blbl1, LXW_BORDER_MEDIUM);
    format_set_bottom(rpt->beams.blbl3, LXW_BORDER_MEDIUM);
    qagen_excel_get_beam_format(rpt->wb, rpt->beams.lbls1, rpt->beams.lbls2, LXW_BORDER_NONE);
    qagen_excel_get_beam_format(rpt->wb, rpt->beams.rlbls1, rpt->beams.rlbls2, LXW_BORDER_MEDIUM);
    rpt->beams.lbls3[0] = qagen_excel_format_create(rpt->wb,
                            FMTARG(format_set_bg_color, SUCCESSALT),
                            FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
                            FMTARG(format_set_align, LXW_ALIGN_CENTER),
                            FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
                            FMTARG(format_set_num_format, &(*("0.0%"))),
                            NULL);
    rpt->beams.lbls3[1] = qagen_excel_format_create(rpt->wb,
                            FMTARG(format_set_bg_color, SUCCESSGRN),
                            FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
                            FMTARG(format_set_align, LXW_ALIGN_CENTER),
                            FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
                            FMTARG(format_set_num_format, &(*("0.0%"))),
                            NULL);
    rpt->beams.rlbls3[0] = qagen_excel_format_create(rpt->wb,
                            FMTARG(format_set_bg_color, SUCCESSALT),
                            FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
                            FMTARG(format_set_right, LXW_BORDER_MEDIUM),
                            FMTARG(format_set_align, LXW_ALIGN_CENTER),
                            FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
                            FMTARG(format_set_num_format, &(*("0.0%"))),
                            NULL);
    rpt->beams.rlbls3[1] = qagen_excel_format_create(rpt->wb,
                            FMTARG(format_set_bg_color, SUCCESSGRN),
                            FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
                            FMTARG(format_set_right, LXW_BORDER_MEDIUM),
                            FMTARG(format_set_align, LXW_ALIGN_CENTER),
                            FMTARG(format_set_align, LXW_ALIGN_VERTICAL_CENTER),
                            FMTARG(format_set_num_format, &(*("0.0%"))),
                            NULL);
    rpt->beams.pctgp = qagen_excel_format_pctgp(rpt->wb);
}


static void qagen_excel_report_init_figfmt(struct report *rpt)
{
    rpt->fig.cap = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_bold, 0),
                        FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
                        NULL);
    rpt->fig.hi = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_top, LXW_BORDER_MEDIUM),
                        NULL);
    rpt->fig.lo = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
                        NULL);
    rpt->fig.lt = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_left, LXW_BORDER_MEDIUM),
                        NULL);
    rpt->fig.rt = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_right, LXW_BORDER_MEDIUM),
                        NULL);
    rpt->fig.bl = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
                        FMTARG(format_set_left, LXW_BORDER_MEDIUM),
                        NULL);
    rpt->fig.br = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_bottom, LXW_BORDER_MEDIUM),
                        FMTARG(format_set_right, LXW_BORDER_MEDIUM),
                        NULL);
    rpt->fig.tl = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_top, LXW_BORDER_MEDIUM),
                        FMTARG(format_set_left, LXW_BORDER_MEDIUM),
                        NULL);
    rpt->fig.tr = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_top, LXW_BORDER_MEDIUM),
                        FMTARG(format_set_right, LXW_BORDER_MEDIUM),
                        NULL);
}


static void qagen_excel_report_init_sigfmt(struct report *rpt)
{
    rpt->sig.lbl = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_align, LXW_ALIGN_RIGHT),
                        NULL);
    rpt->sig.phys = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_bg_color, FIELDBG),
                        FMTARG(format_set_align, LXW_ALIGN_CENTER),
                        FMTARG(format_set_font_size, 9.0),
                        FMTARG(format_set_bottom, LXW_BORDER_THIN),
                        NULL);
    rpt->sig.date = qagen_excel_format_create(rpt->wb,
                        FMTARG(format_set_bg_color, FIELDBG),
                        FMTARG(format_set_align, LXW_ALIGN_CENTER),
                        FMTARG(format_set_font_size, 9.0),
                        FMTARG(format_set_bottom, LXW_BORDER_THIN),
                        FMTARG(format_set_num_format, &(*(SIG_DTFMT))),
                        NULL);
}


static void qagen_excel_report_init(struct report *rpt)
{
    workbook_set_properties(rpt->wb, &(lxw_doc_properties){
        .title    = BK_TITLE,
        .author   = BK_AUTHOR,
        .company  = BK_COMPANY,
        .comments = BK_COMMENT
    });
    qagen_excel_report_init_ptfmt(rpt);
    qagen_excel_report_init_fldfmt(rpt);
    qagen_excel_report_init_beamfmt(rpt);
    qagen_excel_report_init_figfmt(rpt);
    qagen_excel_report_init_sigfmt(rpt);
}


static int qagen_excel_open(const struct qagen_patient *pt, const char *utf8name)
{
    struct report rpt = {
        .wb     = workbook_new(utf8name),
        .mu     = workbook_add_worksheet(rpt.wb, MUSHEET),
        .qa     = workbook_add_worksheet(rpt.wb, QASHEET),
        .nbeams = qagen_patient_num_beams(pt)
    };
    qagen_excel_report_init(&rpt);
    qagen_excel_mu_sheet(pt, &rpt);
    qagen_excel_qa_sheet(pt, &rpt);
    return workbook_close(rpt.wb);
}


int qagen_excel_write(const struct qagen_patient *pt, const wchar_t *filename)
{
    char fcvt[512];
    int res;
    if (wcstombs(fcvt, filename, sizeof fcvt) == (size_t)-1) {
        qagen_error_raise(QAGEN_ERR_SYSTEM, &(const int){ EILSEQ }, L"Failed to convert %s to UTF-8", filename);
        return 1;
    }
    res = qagen_excel_open(pt, fcvt);
    if (res) {
        const char *what = lxw_strerror(res);
        qagen_log_printf(QAGEN_LOG_ERROR, L"xlsxwriter failed: %S", what);
        qagen_error_raise(QAGEN_ERR_RUNTIME, L"Failed to write XLSX file", L"LXW: %S", what);
    }
    return res != 0;
}
