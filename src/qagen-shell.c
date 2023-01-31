#include "qagen-app.h"
#include "qagen-shell.h"
#include "qagen-copy.h"
#include "qagen-patient.h"
#include "qagen-path.h"
#include "qagen-filedlg.h"
#include "qagen-string.h"
#include "qagen-error.h"
#include "qagen-debug.h"
#include "qagen-memory.h"

/** Not used. I use the error state instead to distinguish a cancel from an
 *  error
 */
#define SHELL_CANCEL -2


/** @brief Opens the RTPlan window to allow the user to select the correct one
 *  @param pt
 *      Patient context
 *  @returns Nonzero either on error, or if the user closed the window. Closing
 *      the window is tantamount to cancelling the operation, but does not
 *      raise an error state, so be wary of this quirk
 */
static int qagen_search_rtplan_disambiguate(struct qagen_patient *pt)
{
    struct qagen_rpwnd rpwnd = { 0 };
    const wchar_t **strings;
    int nstrings, res = 1;
    nstrings = qagen_file_beam_strings(pt->rtplan, &strings);
    if (nstrings) {
        res = qagen_rpwnd_show(&rpwnd, nstrings, strings);
        switch (res) {
        case RPWND_ERROR:
        case RPWND_CLOSED:
            res = 1;
            break;
        default:
            pt->rtplan = qagen_file_list_extract(pt->rtplan, res);
            res = 0;
        }
        qagen_file_beam_strings_free(nstrings, strings);
    }
    return res;
}


/** @brief Search for all RP files in @p rspath, then select one of them. After
 *      a successful call to this function, the RP list in the patient context
 *      will contain exactly one node (the selected)
 *  @param pt
 *      Patient context
 *  @param rspath
 *      PATH to RS directory
 *  @returns Nonzero on error or cancel
 */
static int qagen_search_rs_rtplan(struct qagen_patient *pt,
                                  const PATH           *rspath)
{
    static const wchar_t *failmsg = L"Failed to enumerate RTPlan files";
    static const wchar_t *pattern = L"RP*.dcm";
    uint32_t len;
    int res = 0;
    pt->rtplan = qagen_file_enumerate(QAGEN_FILE_DCM_RP, rspath, pattern);
    len = qagen_file_list_len(pt->rtplan);
    qagen_log_printf(QAGEN_LOG_INFO, L"Found %u RP file%s", len, PLFW(len));
    switch (len) {
    case 0:
        if (!qagen_error_state()) {
            qagen_error_raise(QAGEN_ERR_RUNTIME, failmsg, L"No files could be found");
            res = 1;
        }
        break;
    case 1:
        break;
    default:
        res = qagen_search_rtplan_disambiguate(pt);
    }
    return res;
}


/** @brief Enumerates all RD files in @p rspath and accepts only those with a
 *      valid beam number and the same instance UID as the RP file. After a
 *      successful call, the RD list in the patient context will contain the
 *      relevant RD files
 *  @param pt
 *      Patient context
 *  @param rspath
 *      PATH to RS directory
 *  @returns Nonzero on error. Cancelling is not possible at this step
 */
static int qagen_search_rs_rtdose(struct qagen_patient *pt,
                                  const PATH           *rspath)
{
    static const wchar_t *failmsg = L"Failed to enumerate RTDose files";
    static const wchar_t *pattern = L"RD*.dcm";
    const uint32_t xpected = qagen_patient_num_beams(pt);
    unsigned len;
    pt->rtdose = qagen_file_enumerate(QAGEN_FILE_DCM_RD, rspath, pattern);
    len = qagen_file_list_len(pt->rtdose);
    qagen_log_printf(QAGEN_LOG_INFO, L"Found %u RD file%s", len, PLFW(len));
    qagen_file_filter_rd(&pt->rtdose, pt->rtplan);
    len = qagen_file_list_len(pt->rtdose);
    qagen_log_printf(QAGEN_LOG_INFO, L"Filtered to %u RD file%s", len, PLFW(len));
    if (len != xpected) {
        if (!qagen_error_state()) {
            qagen_error_raise(QAGEN_ERR_RUNTIME, failmsg, L"Expected %u beam%s, found %u", xpected, PLFW(xpected), len);
        }
        return 1;
    } else {
        return 0;
    }
}


/** @brief Searches for one or more RTPlan files in @p rspath, selects one,
 *      then finds its associated RTDose files. The results are stored in the
 *      patient structure
 *  @param pt
 *      Patient structure
 *  @param rspath
 *      PATH to RS directory
 *  @returns Nonzero on error/cancel
 *  @note Cancelling is possible from this code path
 */
static int qagen_search_rs_folder(struct qagen_patient *pt,
                                  const PATH           *rspath)
{
    return qagen_search_rs_rtplan(pt, rspath)
        || qagen_search_rs_rtdose(pt, rspath);
}


enum {
    MC2_SEARCH_ERROR = -1,
    MC2_SEARCH_FOUND_NONE,
    MC2_SEARCH_FOUND_DICOM,
    MC2_SEARCH_FOUND_MHD
};


/** @brief Searches first for Dose_Beam* DICOM files. If it does not find any,
 *      it then searches for MHD files
 *  @param pt
 *      Patient context
 *  @param dir
 *      Directory to search
 *  @param state
 *      Search state
 */
static void qagen_search_mc2_types(struct qagen_patient *pt,
                                   const PATH           *dir,
                                   int                  *state)
{
    static const wchar_t *dcmpat = L"Dose_Beam*.dcm";
    static const wchar_t *mhdpat = L"Dose_Beam*.mhd";
    const uint32_t xpected = qagen_patient_num_beams(pt);
    struct qagen_file *head = qagen_file_enumerate(QAGEN_FILE_DCM_DOSEBEAM, dir, dcmpat);
    unsigned len = qagen_file_list_len(head);
    /* First search for DICOMs (the termination condition guarantees that this
    is correct) */
    if (qagen_error_state()) {
        *state = MC2_SEARCH_ERROR;
    } if (len == xpected) {
        *state = MC2_SEARCH_FOUND_DICOM;
        qagen_file_list_free(pt->dose_beam);
        pt->dose_beam = head;
        qagen_log_printf(QAGEN_LOG_INFO, L"Found %u DICOM Dose_Beams%s", len, PLFW(len));
    } else {
        qagen_log_printf(QAGEN_LOG_WARN, L"Found %u DICOM Dose_Beam%s, expected %u", len, PLFW(len), xpected);
        qagen_ptr_nullify(&head, qagen_file_list_free);
    }
    /* Search for MHDs if we have not found anything */
    if (*state == MC2_SEARCH_FOUND_NONE) {
        head = qagen_file_enumerate(QAGEN_FILE_MHD_DOSEBEAM, dir, mhdpat);
        len = qagen_file_list_len(head);
        if (qagen_error_state()) {
            *state = MC2_SEARCH_ERROR;
        } else if (len == xpected) {
            *state = MC2_SEARCH_FOUND_MHD;
            pt->dose_beam = head;
            qagen_log_printf(QAGEN_LOG_INFO, L"Found %u MHD Dose_Beam%s", len, PLFW(len));
        } else {
            qagen_log_printf(QAGEN_LOG_WARN, L"Found %u MHD Dose_Beam%s, expected %u", len, PLFW(len), xpected);
            qagen_file_list_free(head);
        }
    }
}


/** @brief Wrapper for FindNextFile */
static bool qagen_search_mc2_findnext(HANDLE           hfind,
                                      WIN32_FIND_DATA *fdata,
                                      int             *state)
{
    static const wchar_t *failmsg = L"Failed to find next folder in MC2 directory";
    BOOL res = FindNextFile(hfind, fdata);
    if (!res) {
        DWORD lasterr = GetLastError();
        switch (lasterr) {
        case ERROR_NO_MORE_FILES:
            break;
        default:
            qagen_error_raise(QAGEN_ERR_WIN32, &lasterr, failmsg);
            *state = MC2_SEARCH_ERROR;
        }
    }
    return res;
}


/** @brief Perform the search algorithm
 *  @param pt
 *      Patient context
 *  @param fdata
 *      Win32 find data for directories in the MC2 path
 *  @param hfind
 *      Win32 find HANDLE 
 *  @param subdir
 *      Path to be pushed/popped with the searched directory
 *  @param res
 *      Result code, also used as a break condition
 *  @details If the current finddata file is a directory, join it to @p subdir
 *      and then fetch DCM and MHD files therein. The state shall be updated
 *      accordingly
 */
static void qagen_search_mc2_algorithm(struct qagen_patient *pt,
                                       WIN32_FIND_DATA      *fdata,
                                       HANDLE                hfind,
                                       PATH                **subdir,
                                       int                  *state)
{
    bool cont;
    do {
        if (qagen_path_is_subdirectory(fdata)) {
            if (qagen_path_join(subdir, fdata->cFileName)) {
                *state = MC2_SEARCH_ERROR;
            } else {
                qagen_log_printf(QAGEN_LOG_INFO, L"Searching MC2 subdirectory .\\%s", fdata->cFileName);
                qagen_search_mc2_types(pt, *subdir, state);
                qagen_path_remove_filespec(subdir);
            }
        }
        switch (*state) {
        case MC2_SEARCH_ERROR:
        case MC2_SEARCH_FOUND_DICOM:
            cont = false;
            break;
        case MC2_SEARCH_FOUND_NONE:
        case MC2_SEARCH_FOUND_MHD:
            cont = qagen_search_mc2_findnext(hfind, fdata, state);
            break;
        }
    } while (cont);
}


/** @brief Wrapper for FindFirstFileEx
 *  @param mc2path
 *      PATH to MC2 base directory
 *  @param fdata
 *      WIN32_FIND_DATA used for iterating subfolders
 *  @returns A findfirst HANDLE used for iterating the subfolders, or
 *      INVALID_HANDLE_VALUE. If an invalid handle is returned, 
 */
static HANDLE qagen_search_mc2_findfirst(const PATH      *mc2path,
                                         WIN32_FIND_DATA *fdata)
{
    static const wchar_t *failmsg = L"Failed to open MC2 directory search handle";
    PATH *wildcard = qagen_path_duplicate(mc2path);
    HANDLE res = INVALID_HANDLE_VALUE;
    if (wildcard && !qagen_path_join(&wildcard, L"*")) {
        res = FindFirstFileEx(wildcard->buf,
                              FindExInfoStandard,
                              fdata,
                              FindExSearchLimitToDirectories,
                              NULL,
                              0);
        if (res == INVALID_HANDLE_VALUE) {
            DWORD lasterr = GetLastError();
            switch (lasterr) {
            case ERROR_FILE_NOT_FOUND:
                qagen_log_puts(QAGEN_LOG_WARN, L"MC2 path does not contain subfolders");
                break;
            case ERROR_PATH_NOT_FOUND:
                qagen_log_puts(QAGEN_LOG_WARN, L"Expected MC2 path does not exist");
                break;
            default:
                /* Anything else is an actual error */
                qagen_error_raise(QAGEN_ERR_WIN32, &lasterr, failmsg);
            }
        }
    }
    qagen_path_free(wildcard);
    return res;
}


/** @brief Searches all possible subdirectories of @p mc2path for Dose_Beams
 *  @details ~~The first folder searched is Outputs.~~ The algorithm is:
 *      - Search for DICOM files first
 *      - If we don't have enough DICOM files, search for MHD files
 *      - Regardless of whether we found enough MHD files, we continue to the
 *        next folder
 *      - Termination occurs when we either find enough DICOM files, or run out
 *        of folders to check
 * 
 *  @note There is only one list for Dose_Beam files in the patient context. It
 *      may contain either DICOM or MHD files; A list of MHD files may be
 *      overwritten by a list of DICOM files (which will then terminate the
 *      search). If we cannot find DICOM files, we will simply accept the first
 *      set of MHD files that we find, so no need to check for them if we have
 *      already found some
 */
static int qagen_search_mc2_subdirs(struct qagen_patient *pt,
                                    const PATH           *mc2path)
{
    WIN32_FIND_DATA fdata;
    HANDLE hfind = qagen_search_mc2_findfirst(mc2path, &fdata);
    PATH *subdir = qagen_path_duplicate(mc2path);
    int state = (qagen_error_state()) ? MC2_SEARCH_ERROR : MC2_SEARCH_FOUND_NONE;
    if (hfind != INVALID_HANDLE_VALUE && subdir) {
        state = MC2_SEARCH_FOUND_NONE;
        qagen_search_mc2_algorithm(pt, &fdata, hfind, &subdir, &state);
    }
    if (hfind != INVALID_HANDLE_VALUE) {
        FindClose(hfind);
    }
    qagen_path_free(subdir);
    return state;
}


/** @brief Searches the MC2 base directory for RD template files. If any exist,
 *      it simply takes the first one found
 */
static int qagen_search_mc2_template(struct qagen_patient *pt,
                                     const PATH           *mc2path)
{
    static const wchar_t *templt = L"*template*.dcm";   /* This might be bad */
    uint32_t len;
    pt->rd_template = qagen_file_enumerate(QAGEN_FILE_DCM_RD, mc2path, templt);
    len = qagen_file_list_len(pt->rd_template);
    qagen_log_printf(QAGEN_LOG_INFO, L"Found %u RD template%s", len, PLFW(len));
    if (qagen_error_state()) {
        return MC2_SEARCH_ERROR;
    } else if (len == 0) {
        return MC2_SEARCH_FOUND_NONE;
    } else {
        pt->rd_template = qagen_file_list_extract(pt->rd_template, 0);
        return MC2_SEARCH_FOUND_DICOM;
    }
}


/** @brief Searches for Dose_Beam files in *every* subdirectory of @p mc2path.
 *      If it can only find MHD files in the necessary amount, it will search
 *      for an RTDose template file in @p mc2path. The results are stored in
 *      the patient struct
 *  @param pt
 *      Patient struct
 *  @param mc2path
 *      PATH to MC2 root directory. Note that this directory should *not* be
 *      the Outputs directory, but its parent directory. We rename the Outputs
 *      folder very frequently, in an attempt to crash+restart the simulator.
 *      Searching every folder in this path allows us to simply rename the
 *      folder and forget about it
 *  @returns Nonzero on error. Returns zero if it doesn't find anything
 *  @note This will only succeed if it finds the correct number of files of
 *      either type. If it can only find MHD files, it *must* find an RD
 *      template file (might change this later). If it cannot successfully find
 *      all of the files it needs for a particular configuration, the lists are
 *      destroyed and pt->dose_beam and pt->rd_template will both be NULL
 */
static int qagen_search_mc2_folder(struct qagen_patient *pt,
                                   const PATH           *mc2path)
{
    switch (qagen_search_mc2_subdirs(pt, mc2path)) {
    case MC2_SEARCH_ERROR:
        return 1;
    case MC2_SEARCH_FOUND_MHD:
        switch (qagen_search_mc2_template(pt, mc2path)) {
        case MC2_SEARCH_ERROR:
            return 1;
        case MC2_SEARCH_FOUND_NONE:
            /* Just delete the dose_beam list, can't convert without template */
            qagen_log_puts(QAGEN_LOG_WARN, L"Found MHD files, but no RD template");
            qagen_ptr_nullify(&pt->dose_beam, qagen_file_list_free);
            /* FALLTHRU */
        default:
            return 0;
        }
    case MC2_SEARCH_FOUND_NONE:
    case MC2_SEARCH_FOUND_DICOM:
    default:
        return 0;
    }
}


/** @brief Search for an RP file, then RS files, then Dose_Beams
 *  @param[out] pt
 *      Patient context
 *  @param rspath
 *      Selected RS path
 *  @param mc2path
 *      Computed MC2 path
 *  @returns Nonzero on error/cancel
 */
static int qagen_search_folders(struct qagen_patient *pt,
                                const PATH           *rspath,
                                const PATH           *mc2path)
{
    if (qagen_search_rs_folder(pt, rspath)
     || qagen_search_mc2_folder(pt, mc2path)) {
        return 1;
    }
    return 0;
}


/** @brief Duplicates the RS path, jumps up two directories, then joins MC2 and
 *      <dpyname>~MC2
 *  @param rspath
 *      PATH to RS directory
 *  @param dpyname
 *      Display name of the RS folder
 *  @returns Assumed PATH to the MC2 directory, or NULL on error
 *  @note The resulting path does not need to exist, and if it does not, this
 *      function will not raise an error state
 */
static PATH *qagen_shell_get_mc2path(const PATH *rspath, const wchar_t *dpyname)
{
    PATH *res = NULL;
    if (rspath) {
        wchar_t *base = qagen_string_createf(L"%s~MC2", dpyname);
        if (base) {
            res = qagen_path_duplicate(rspath);
            if (res) {
                qagen_path_remove_filespec(&res);
                qagen_path_remove_filespec(&res);
                if (qagen_path_join(&res, L"MC2")
                 || qagen_path_join(&res, base)) {
                    qagen_ptr_nullify(&res, qagen_path_free);
                }
            }
            qagen_free(base);
        }
    }
    return res;
}


/** @brief Initializes a patient context using the path @p rsstr, then searches for
 *  files, and attempts a transfer
 *  @note IMPORTANT: This function ***MUST*** return nonzero if an error
 *      occurred, OR if the user cancelled the operation. The caller will
 *      determine which from the thread's error state
 *  @param rsstr
 *      Path string selected by user
 *  @param dpyname
 *      Display name of the selected folder (this gets tokenized)
 *  @param ptidx
 *      The index of this patient
 *  @param totalpts
 *      The total number of selected patients
 *  @returns Nonzero on error/cancel
 */
static int qagen_shell_init_patient(const wchar_t *rsstr, wchar_t *dpyname,
                                    DWORD          ptidx, DWORD    totalpts)
{
    struct qagen_patient pt = { 0 };
    PATH *rspath = qagen_path_create(rsstr);
    PATH *mc2path = qagen_shell_get_mc2path(rspath, dpyname);
    int res = 1;
    if (mc2path) {
        res = qagen_patient_init(&pt, dpyname, ptidx, totalpts);
        if (!res) {
            res = qagen_search_folders(&pt, rspath, mc2path)
               || qagen_patient_create_qa(&pt)
               || qagen_copy_patient(&pt);
            qagen_patient_cleanup(&pt);
        }
    }
    qagen_path_free(mc2path);
    qagen_path_free(rspath);
    return res;
}


/** @brief Attempts to create a QA folder for each of the selected patients
 *  @param dlg
 *      File dialog containing the selected path strings
 *  @returns The result code
 *  @note OK, if any windows were closed past this point, we should treat that
 *      as a single patient cancel, and not a close. Basically: If we get to
 *      this point, there is no closing the application, only cancelling a
 *      single operation 
 */
static qagen_shlres_t qagen_shell_create_patients(struct qagen_filedlg *dlg)
{
    wchar_t *rspath, *dpyname;
    qagen_shlres_t res = SHELL_NORMAL;
    for (DWORD i = 0; i < dlg->count && !res; i++) {
        if (!qagen_filedlg_path(dlg, i, &rspath, &dpyname)) {
            if (qagen_shell_init_patient(rspath, dpyname, i, dlg->count)) {
                res = (qagen_error_state()) ? SHELL_ERROR : SHELL_NORMAL;
            }
        } else {
            res = SHELL_ERROR;
        }
    }
    return res;
}


qagen_shlres_t qagen_shell_run(void)
{
    struct qagen_filedlg fdlg = { 0 };
    qagen_shlres_t res;
    switch (qagen_filedlg_show(&fdlg)) {
    case FILEDLG_CLOSED:
        res = SHELL_CLOSED;
        break;
    case FILEDLG_ERROR:
        res = SHELL_ERROR;
        break;
    case FILEDLG_NORMAL:
        res = qagen_shell_create_patients(&fdlg);
        break;
    }
    qagen_filedlg_destroy(&fdlg);
    qagen_debug_memtable_log_extant();
    return res;
}