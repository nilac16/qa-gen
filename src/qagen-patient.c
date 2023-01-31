#include <ctype.h>
#include <stdio.h>
#include "qagen-patient.h"
#include "qagen-json.h"
#include "qagen-excel.h"
#include "qagen-memory.h"
#include "qagen-log.h"
#include "qagen-error.h"


/** @brief Jumps to the first non-whitespace character in @p nptr. If @p nptr
 *      is empty or contains only whitespace, this will return a pointer to the
 *      null term  
 *  @param nptr
 *      Pointer to string to the stripped of leading whitespace
 *  @returns @p nptr stripped of leading whitespace, as determined by
 *      iswspace(3)
 */
static const wchar_t *ltrim(const wchar_t *nptr)
{
    while (iswspace(*nptr)) {
        nptr++;
    }
    return nptr;
}


/** @brief Compares the first wchar in @p str to @p lead, then strips leading
 *      whitespace and reads a double, placing it in @p dub. @p str is then
 *      advanced to the first non-whitespace character after the double could
 *      no longer be read
 *  @param str
 *      Pointer to pointer to string to be processed
 *  @param lead
 *      Expected leading character. If @p str does not begin with this symbol
 *      on entry, returns 1
 *  @param dub
 *      Pointer to location where the expected value should be read. If this
 *      cannot be read, returns 1
 *  @returns Nonzero if it cannot complete. If this operation succeeds, then
 *      @p str is in a state that allows chaining this call
 */
static int qagen_patient_parse_dub(const wchar_t **str,
                                   wchar_t         lead,
                                   double         *dub)
{
    wchar_t *endptr;
    if (**str != lead) {
        return 1;
    } else {
        *str += 1;
        *dub = wcstod(*str, &endptr);
        if (*str == endptr) {
            return 1;
        } else {
            *str = ltrim(endptr);
            return 0;
        }
    }
}


static int qagen_patient_parse_iso(struct qagen_patient *pt)
{
    static const wchar_t *failctx = L"Could not parse isocenter string";
    static const wchar_t *failfmt = L"%s is invalid";
    const wchar_t *tptr = ltrim(pt->tokens[PT_TOK_ISO]);
    if (!(qagen_patient_parse_dub(&tptr, L'(', &pt->iso[0])
       || qagen_patient_parse_dub(&tptr, L',', &pt->iso[1])
       || qagen_patient_parse_dub(&tptr, L',', &pt->iso[2]))) {
        if (*tptr == L')' && !*ltrim(tptr + 1)) {
            return 0;
        }
    }
    qagen_error_raise(QAGEN_ERR_RUNTIME, failctx, failfmt, pt->tokens[PT_TOK_ISO]);
    return 1;
}


static int qagen_patient_tokenize(struct qagen_patient *pt,
                                  wchar_t              *dpy)
{
    static const wchar_t *failctx = L"Could not parse folder name";
    static const wchar_t *failfmt = L"Found %zu %s, expected %zu";
    static const wchar_t delim = '~';
    wchar_t *r = wcschr(dpy, delim);
    size_t i = 0;
    while (r) {
        if (i < PT_N_TOKENS) {
            pt->tokens[i] = dpy;
        }
        i++;
        *r = '\0';
        dpy = r + 1;
        r = wcschr(dpy, delim);
    }
    if (i < PT_N_TOKENS) {
        pt->tokens[i] = dpy;
    }
    if (++i != PT_N_TOKENS) {
        const wchar_t *tok = (i == 1) ? L"token" : L"tokens";
        qagen_error_raise(QAGEN_ERR_RUNTIME, failctx, failfmt, i, tok, (size_t)PT_N_TOKENS);
        return 1;
    } else {
        return qagen_patient_parse_iso(pt);
    }
}


/** @brief Dangerous (just be careful lol)
 *  @note Actually tho, this writes to the first two indices in the worst case,
 *      none in the "best," so make sure that @p dst has at least 3 more spaces
 */
static size_t write_initials(wchar_t *dst, const wchar_t *name)
{
    wchar_t *const base = dst;
    if (*name) {
        *dst++ = towupper(*name++);
        if (*name) {
            *dst++ = towlower(*name);
        }
    }
    return (size_t)(dst - base);
}


static size_t write_isocomp(wchar_t *dst, size_t dstlen, double x)
{
    if (x) {
        return swprintf(dst, dstlen, L"%.1f", x);
    } else {
        *dst = L'0';
        return (size_t)1;
    }
}


/** > Do not end a file or directory name with a space or a period.
 *  https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file
 */
static bool invalid_trailing_char(wchar_t c)
{
    return iswspace(c) || c == L'.';
}


/** @brief Validates the folder name to be sure that it is a valid path string.
 *      If it finds an illegal char, it replaces it with '-'
 *  @param name
 *      Path string
 *  @returns Nonzero if the remaining path is empty
 *  @note All of this information is pulled from the RS folder display name...
 *      which is a valid path string. There is a possibility of trailing
 *      whitespace or periods though, neither of which are not allowed
 */
static int qagen_patient_validate_foldername(wchar_t *name)
{
    wchar_t *const base = name;
    for (; *name; name++) {
        if (!qagen_path_char_isvalid(*name)) {
            *name = L'-';
        }
    }
    do {
        name--;
        if (!invalid_trailing_char(*name)) {
            break;
        }
        *name = L'\0';
    } while (name > base);
    return *base == L'\0';
}


static int qagen_patient_generate_foldername(struct qagen_patient *pt)
{
    size_t i = 0;
    i += write_initials(pt->foldername + i, pt->tokens[PT_TOK_LNAME]);
    i += write_initials(pt->foldername + i, pt->tokens[PT_TOK_FNAME]);
    pt->foldername[i++] = L'_';
    pt->foldername[i++] = L'(';
    i += write_isocomp(pt->foldername + i, BUFLEN(pt->foldername) - i, pt->iso[0]);
    pt->foldername[i++] = L',';
    i += write_isocomp(pt->foldername + i, BUFLEN(pt->foldername) - i, pt->iso[1]);
    pt->foldername[i++] = L',';
    i += write_isocomp(pt->foldername + i, BUFLEN(pt->foldername) - i, pt->iso[2]);
    swprintf(pt->foldername + i, BUFLEN(pt->foldername) - i, L")-%s", pt->tokens[PT_TOK_BEAMSET]);
    return qagen_patient_validate_foldername(pt->foldername);
}


int qagen_patient_init(struct qagen_patient *pt,    wchar_t *dpyname,
                       DWORD                 ptidx, DWORD    pttotal)
{
    pt->pt_idx = ptidx;
    pt->pt_tot = pttotal;
    if (qagen_patient_tokenize(pt, dpyname)
     || qagen_patient_generate_foldername(pt)) {
        return 1;
    } else {
        qagen_log_printf(QAGEN_LOG_INFO, L"Selected patient %s, %s", pt->tokens[PT_TOK_LNAME], pt->tokens[PT_TOK_FNAME]);
    }
    return 0;
}


void qagen_patient_cleanup(struct qagen_patient *pt)
{
    qagen_file_list_free(pt->rtplan);
    qagen_file_list_free(pt->rtdose);
    qagen_file_list_free(pt->dose_beam);
    qagen_file_list_free(pt->rd_template);
    qagen_path_free(pt->basepath);
    SecureZeroMemory(pt, sizeof *pt);
}


uint32_t qagen_patient_num_beams(const struct qagen_patient *pt)
{
    return pt->rtplan->data.rp.nbeams;
}


static int qagen_patient_create_mu(struct qagen_patient *pt)
{
    static const wchar_t *failmsg = L"CreateDirectory failed to create this patient's MU folder";
    if (!qagen_path_join(&pt->basepath, L"MU")) {
        if (!CreateDirectory(pt->basepath->buf, NULL)) {
            DWORD lasterr = GetLastError();
            if (lasterr != ERROR_ALREADY_EXISTS) {
                qagen_error_raise(QAGEN_ERR_WIN32, &lasterr, failmsg);
                return 1;
            }
        }
        qagen_path_remove_filespec(&pt->basepath);
        return 0;
    }
    return 1;
}


static int qagen_patient_create_base(struct qagen_patient *pt)
{
    static const wchar_t *failmsg = L"CreateDirectory failed to create this patient's folder";
    if (!qagen_path_join(&pt->basepath, pt->foldername)) {
        if (!CreateDirectory(pt->basepath->buf, NULL)) {
            DWORD lasterr = GetLastError();
            if (lasterr == ERROR_ALREADY_EXISTS) {
                qagen_log_printf(QAGEN_LOG_WARN, L"Patient folder %s already exists", pt->foldername);
            } else {
                qagen_error_raise(QAGEN_ERR_WIN32, &lasterr, failmsg);
                return 1;
            }
        }
        return qagen_patient_create_mu(pt);
    }
    return 1;
}


static int qagen_patient_create_dir(struct qagen_patient *pt)
{
    static const wchar_t *failmsg = L"CreateDirectory failed on patients' root dir";
    pt->basepath = qagen_path_create(L".\\patients");
    if (pt->basepath) {
        if (!CreateDirectory(pt->basepath->buf, NULL)) {
            DWORD lasterr = GetLastError();
            if (lasterr != ERROR_ALREADY_EXISTS) {
                qagen_error_raise(QAGEN_ERR_WIN32, &lasterr, failmsg);
                return 1;
            }
        }
        return qagen_patient_create_base(pt);
    }
    return 1;
}


static int qagen_patient_create_json(struct qagen_patient *pt)
{
    static const wchar_t *fname = L"patient.json";
    int res = 1;
    if (!qagen_path_join(&pt->basepath, fname)) {
        res = qagen_json_write(pt, pt->basepath->buf);
        qagen_path_remove_filespec(&pt->basepath);
    }
    if (!res) {
        qagen_log_printf(QAGEN_LOG_INFO, L"Created %s", fname);
    }
    return res;
}


static int qagen_patient_create_excel(struct qagen_patient *pt)
{
    wchar_t buf[26]; /* The max possible for this locale (doesn't even matter because swprintf is bounds-checked anyway, oh well) */
    int res = 1;
    swprintf(buf, BUFLEN(buf), L"QA_%uFields.xlsx", qagen_patient_num_beams(pt));
    if (!qagen_path_join(&pt->basepath, buf)) {
        res = qagen_excel_write(pt, pt->basepath->buf);
        qagen_path_remove_filespec(&pt->basepath);
    }
    if (!res) {
        qagen_log_printf(QAGEN_LOG_INFO, L"Created %s", buf);
    }
    return res;
}


int qagen_patient_create_qa(struct qagen_patient *pt)
{
    return qagen_patient_create_dir(pt)
        || qagen_patient_create_json(pt)
        || qagen_patient_create_excel(pt);
}
