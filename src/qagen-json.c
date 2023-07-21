/** It appears that I just didn't feel like commenting the static functions in
 *  this file in a meaningful way...
 */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "qagen-json.h"
#include "qagen-error.h"
#include "qagen-string.h"
#include "qagen-memory.h"
#include "qagen-log.h"
#include <json-c/json.h>

#define PT_KEY_ISO    "isocenter"
#define PT_KEY_FIELDS "fields"

/** json-c provides no evident error-handling mechanisms, so I log verbosely
 *  from this file
 */


static const char *pt_keys[] = {
    "patient_ID",
    "last_name",
    "first_name",
    "plan_name",
    "beamset",
    PT_KEY_ISO
};


enum {
    PT_FLD_TXMACHINE = 0,
    PT_FLD_BEAMNAME,
    PT_FLD_BEAMDESC,
    PT_FLD_RNGSHFTR,
    PT_FLD_METERSET,
    PT_N_FLD_TAGS
};


static const char *field_keys[] = {
    "treatment_machine",
    "beam_name",
    "beam_description",
    "energy_absorber",
    "meterset"
};

//static_assert(BUFLEN(pt_keys) == PT_TOK_ISO, "Mismatched patient keys");
static_assert(BUFLEN(field_keys) == PT_N_FLD_TAGS, "Mismatched field keys");


/** @brief Will only return if it succeeds */
static json_object *qagen_json_wstring_node(const wchar_t *str, jmp_buf env)
{
    static const wchar_t *failfmt = L"Failed to convert \"%s\" to UTF-8";
    char cvt[512];
    json_object *res = NULL;
    size_t out;

    out = wcstombs(cvt, str, BUFLEN(cvt));
    if (out == (size_t)-1) {
        /* qagen_error_raise(QAGEN_ERR_SYSTEM, &(const int){ EILSEQ }, failfmt, str); */
        qagen_log_puts(QAGEN_LOG_ERROR, L"Illegal sequence encountered while encoding JSON node");
    } else {
        res = json_object_new_string(cvt);
        if (res) {
            return res;
        } else {
            qagen_log_puts(QAGEN_LOG_ERROR, L"Failed to create JSON string node");
        }
    }
    longjmp(env, 1);
    return NULL;
}


/** @brief Only returns if it succeeds */
static json_object *qagen_json_double_node(double x, jmp_buf env)
{
    json_object *res;

    res = json_object_new_double(x);
    if (!res) {
        qagen_log_printf(QAGEN_LOG_ERROR, L"Failed to create JSON node containing %f", x);
        longjmp(env, 1);
    }
    return res;
}


static json_object *qagen_json_rangeshifter_node(int rsid, jmp_buf env)
{
    json_object *res;

    res = json_object_new_int(rsid);
    if (!res) {
        qagen_log_puts(QAGEN_LOG_ERROR, L"Failed to create JSON node for range shifter ID");
        longjmp(env, 1);
    }
    return res;
}


/** @brief No error checks */
static int qagen_json_meterset_serializer(json_object     *jso,
                                          struct printbuf *pb,
                                          int              level,
                                          int              flags)
{
    (void)level;
    (void)flags;
    return sprintbuf(pb, "%.5f", json_object_get_double(jso));
}


static json_object *qagen_json_meterset_node(double x, jmp_buf env)
{
    json_object *res;

    res = qagen_json_double_node(x, env);
    json_object_set_serializer(res, qagen_json_meterset_serializer, NULL, NULL);
    return res;
}


/** @brief Attaches all of the tokens to root in order */
static void qagen_json_make_tokens(const struct qagen_patient *pt,
                                   json_object                *root,
                                   jmp_buf                     env)
{
    json_object *node;
    uint32_t i;

    for (i = 0; i < PT_TOK_ISO; i++) {
        node = qagen_json_wstring_node(pt->tokens[i], env);
        if (json_object_object_add(root, pt_keys[i], node) < 0) {
            qagen_log_printf(QAGEN_LOG_ERROR, L"Failed adding token node %u", i);
            json_object_put(node);
            longjmp(env, 1);
        }
    }
}


static void qagen_json_make_iso(const struct qagen_patient *pt,
                                json_object                *root,
                                jmp_buf                     env)
{
    json_object *arr;

    arr = json_object_new_array();
    if (arr) {
        json_object_array_add(arr, qagen_json_double_node(pt->iso[0], env));
        json_object_array_add(arr, qagen_json_double_node(pt->iso[1], env));
        json_object_array_add(arr, qagen_json_double_node(pt->iso[2], env));
        if (!json_object_object_add(root, PT_KEY_ISO, arr)) {
            return;
        } else {
            qagen_log_puts(QAGEN_LOG_ERROR, L"Failed adding isocenter array to JSON");
        }
    } else {
        qagen_log_puts(QAGEN_LOG_ERROR, L"Failed to create JSON isocenter array");
    }
    longjmp(env, 1);
}


static json_object *qagen_json_single_field(const struct qagen_rtbeam *beam,
                                            jmp_buf                    env)
{
    json_object *root;

    root = json_object_new_object();
    if (root) {
        if (!json_object_object_add(root, field_keys[PT_FLD_TXMACHINE], qagen_json_wstring_node(beam->machine, env))
         && !json_object_object_add(root, field_keys[PT_FLD_BEAMNAME], qagen_json_wstring_node(beam->name, env))
         && !json_object_object_add(root, field_keys[PT_FLD_BEAMDESC], qagen_json_wstring_node(beam->desc, env))
         && !json_object_object_add(root, field_keys[PT_FLD_RNGSHFTR], qagen_json_rangeshifter_node(beam->rs_id, env))
         && !json_object_object_add(root, field_keys[PT_FLD_METERSET], qagen_json_meterset_node(beam->meterset, env))) {
            return root;
        } else {
            qagen_log_puts(QAGEN_LOG_ERROR, L"Failed adding a JSON field component");
            json_object_put(root);
        }
    } else {
        qagen_log_puts(QAGEN_LOG_ERROR, L"Failed to create JSON field node");
    }
    longjmp(env, 1);
}


static void qagen_json_make_fields(const struct qagen_patient *pt,
                                   json_object                *root,
                                   jmp_buf                     env)
{
    const struct qagen_rtbeam *beam;
    json_object *arr;
    uint32_t n, i;

    n = qagen_patient_num_beams(pt);
    arr = json_object_new_array();
    if (arr) {
        for (i = 0; i < n; i++) {
            beam = &pt->rtplan->data.rp.beam[i];
            json_object_array_add(arr, qagen_json_single_field(beam, env));
        }
        if (!json_object_object_add(root, PT_KEY_FIELDS, arr)) {
            return;
        } else {
            qagen_log_puts(QAGEN_LOG_ERROR, L"Failed adding field array to JSON");
        }
    } else {
        qagen_log_puts(QAGEN_LOG_ERROR, L"Failed creating JSON field array");
    }
    longjmp(env, 1);
}


/** @brief setjmp happens here */
static json_object *qagen_json_make(const struct qagen_patient *pt)
{
    json_object *root;
    jmp_buf env;

    root = json_object_new_object();
    if (root) {
        if (setjmp(env)) {
            qagen_ptr_nullify(&root, json_object_put);
        } else {
            qagen_json_make_tokens(pt, root, env);
            qagen_json_make_iso(pt, root, env);
            qagen_json_make_fields(pt, root, env);
        }
    } else {
        qagen_log_puts(QAGEN_LOG_ERROR, L"Failed to create JSON root node");
    }
    return root;
}


int qagen_json_write(const struct qagen_patient *pt, const wchar_t *filename)
{
    static const wchar_t *failmsg = L"Failed to open JSON file for writing";
    const int jflags = JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED;
    int res = 0;
    json_object *root;
    FILE *fp;

    root = qagen_json_make(pt);
    if (root) {
        fp = _wfopen(filename, L"w");
        if (fp) {
            fputs(json_object_to_json_string_ext(root, jflags), fp);
            json_object_put(root);
            fclose(fp);
        } else {
            qagen_log_printf(QAGEN_LOG_ERROR, L"Could not _wfopen %s: %s", filename, _wcserror(errno));
            /* qagen_error_raise(QAGEN_ERR_SYSTEM, NULL, failmsg); */
            /* res = 1; */
        }
    } else {
        qagen_log_puts(QAGEN_LOG_ERROR, L"Attempt to build JSON failed");
    }
    return res;
}


/** @brief Gets the size of the file with HANDLE @p hfile
 *  @param hfile
 *      File HANDLE
 *  @param len
 *      Length of mapping
 *  @returns @p hfile on success. On failure, closes @p hfile and returns
 *      INVALID_HANDLE_VALUE
 */
static HANDLE qagen_json_handle_len(HANDLE hfile, size_t *len)
{
    static const wchar_t *failmsg = L"Failed to get JSON file length";
    LARGE_INTEGER buf;

    if (GetFileSizeEx(hfile, &buf)) {
        *len = buf.QuadPart;
    } else {
        qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
        CloseHandle(hfile);
        hfile = INVALID_HANDLE_VALUE;
    }
    return hfile;
}


/** @brief Opens a HANDLE to the file, for later _read calls
 *  @param filename
 *      Path to file
 *  @param len
 *      On success, the length of the file is written here. May NOT be NULL
 *  @returns A HANDLE to the opened file, or INVALID_HANDLE_VALUE on error
 */
static HANDLE qagen_json_get_handle(const wchar_t *filename, size_t *len)
{
    static const wchar_t *failmsg = L"Failed to open JSON file";
    HANDLE res;

    res = CreateFile(filename,
                     GENERIC_READ,
                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     NULL,
                     OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL,
                     NULL);
    if (res == INVALID_HANDLE_VALUE) {
        qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
    } else {
        res = qagen_json_handle_len(res, len);
    }
    return res;
}


/** @brief Loads the data contained by file at @p filename into a byte buffer
 *  @param filename
 *      Path to file
 *  @returns A pointer to a buffer containing a *copy* of the contents of the
 *      file, or NULL on error
 */
static char *qagen_json_load_file(const wchar_t *filename)
{
    static const wchar_t *failmsg = L"Failed to read JSON contents";
    char *res = NULL;
    HANDLE hfile;
    size_t len;

    hfile = qagen_json_get_handle(filename, &len);
    if (hfile != INVALID_HANDLE_VALUE) {
        res = qagen_malloc(sizeof *res * len);
        if (res) {
            if (!ReadFile(hfile, res, (DWORD)len, &(DWORD){ 0 }, NULL)) {
                qagen_error_raise(QAGEN_ERR_WIN32, NULL, failmsg);
                qagen_ptr_nullify(&res, qagen_free);
            }
        }
        CloseHandle(hfile);
    }
    return res;
}


/** @brief Reads isocenter information
 *  @param pt
 *      Patient context, destination
 *  @param val
 *      Isocenter array
 *  @returns Nonzero on error
 */
static int qagen_json_dfs_load_iso(struct qagen_patient *pt,
                                   json_object          *val)
{
    json_object *elem;
    size_t i, n;

    if (json_object_get_type(val) != json_type_array) {
        /* Not stored properly */
        qagen_error_raise(QAGEN_ERR_RUNTIME, L"Cannot read isocenter information from JSON", L"Iso is not formatted as an array (%S)", json_object_get_string(val));
        return 1;
    }
    n = json_object_array_length(val);
    if (n != 3) {
        /* Malformed iso */
        qagen_error_raise(QAGEN_ERR_RUNTIME, L"Cannot read isocenter information from JSON", L"Expected 3 components, found %zu", n);
        return 1;
    }
    for (i = 0; i < 3; i++) {
        elem = json_object_array_get_idx(val, i);
        errno = 0;
        pt->iso[i] = json_object_get_double(elem);
        if (errno) {
            qagen_error_raise(QAGEN_ERR_SYSTEM, NULL, L"Cannot read iso component");
            return 1;
        }
    }
    return 0;
}


/** @brief Loads the key specified by @p kyidx and @p val into the patient info
 *  @param pt
 *      Patient context
 *  @param kyidx
 *      Key index
 *  @param val
 *      JSON value for this key
 *  @returns Nonzero on error
 */
static int qagen_json_dfs_load_key(struct qagen_patient *pt,
                                   unsigned              kyidx,
                                   json_object          *val)
{
    const char *vstring;

    switch (kyidx) {
    case PT_TOK_ISO:
        /* Special handling */
        return qagen_json_dfs_load_iso(pt, val);
    default:
        /* Copy the token to backing store and assign the token index */
        vstring = json_object_get_string(val);
        pt->tokstore[kyidx] = qagen_string_utf16cvt(vstring);
        if (!pt->tokstore[kyidx]) {
            return 1;
        }
        pt->tokens[kyidx] = pt->tokstore[kyidx];
        break;
    }
    return 0;
}


/** @brief Swaps the values at @p x and @p y */
static void intxchg(unsigned *restrict x, unsigned *restrict y)
{
    unsigned tmp = *x;

    *x = *y;
    *y = tmp;
}


/** @brief Check the provided key-value pair for inclusion in the remaining
 *      required patient keys
 *  @param pt
 *      Patient context
 *  @param remky
 *      Remaining key indices
 *  @param remct
 *      Remaining key count
 *  @param key
 *      Test key
 *  @param val
 *      Test value
 *  @returns Nonzero on error
 */
static int qagen_json_dfs_keycheck(struct qagen_patient *pt,
                                   unsigned              remky[],
                                   unsigned    *restrict remct,
                                   const char           *key,
                                   json_object          *val)
{
    unsigned i, keyidx;

    for (i = 0; i < *remct; i++) {
        keyidx = remky[i];
        if (!strcmp(key, pt_keys[keyidx])) {
            if (qagen_json_dfs_load_key(pt, keyidx, val)) {
                return 1;
            }
            *remct -= 1;
            intxchg(&remky[i], &remky[*remct]);
            break;
        }
    }
    return 0;
}


/** @brief DFS on the JSON tree and read the first instance of each key that we
 *      are searching for
 *  @param pt
 *      Patient context
 *  @param node
 *      Subtree root
 *  @param remky
 *      Remaining key indices
 *  @param remct
 *      Count of remaining key indices
 *  @returns Nonzero on error
 */
static int qagen_json_dfs(struct qagen_patient *pt,
                          json_object          *node,
                          unsigned              remky[],
                          unsigned    *restrict remct)
{
    /* int itercnt = 0; */
    /* unsigned i, n; */
    int res = 0;

    if (!node || json_object_get_type(node) != json_type_object) {
        /* Discard nodes that are not objects? */
        return 0;
    }
    json_object_object_foreach(node, key, val) {
        switch (json_object_get_type(val)) {
        /* omg im such a hack wtf */
        /* case json_type_array:
            n = json_object_array_length(val);
            for (i = 0; i < n && !res; i++) {
                val = json_object_array_get_idx(val, i);
                switch (json_object_get_type(val)) {
                case json_type_object:
                    res = qagen_json_dfs(pt, val, remky, remct);
                    break;
                default:
                    res = qagen_json_dfs_keycheck(pt, remky, remct, key, val);
                    break;
                }
            }
            break; */
        case json_type_object:
            res = qagen_json_dfs(pt, val, remky, remct);
            break;
        default:
            res = qagen_json_dfs_keycheck(pt, remky, remct, key, val);
        }
        if (res) {
            break;
        }
    }
    return res;
}


/** @brief Searches for patient keys in the JSON tree rooted at @p root
 *  @param pt
 *      Patient context, destination
 *  @param root
 *      Root of JSON tree
 *  @returns Nonzero on error
 */
static int qagen_json_load(struct qagen_patient *pt, json_object *root)
/** Not sure what the idiom is for key lookup using JSON-C, but I'm just gonna
 *  DFS to find them
 */
{
    /* Index map to the patient keys. As keys are found, swap them with the end
    and decrement the counter */
    unsigned rem[] = { 0, 1, 2, 3, 4, 5 }, count = BUFLEN(rem);
    wchar_t ctx[256];

    if (qagen_json_dfs(pt, root, rem, &count)) {
        return 1;
    } else if (count) {
        swprintf(ctx, BUFLEN(ctx), L"JSON is missing %u required keys", count);
        qagen_error_raise(QAGEN_ERR_RUNTIME, ctx, L"First missing key is %S", pt_keys[rem[0]]);
    }
    return count != 0;
}


/** @brief DFS from @p root, searching for a node with key @p key
 *  @param root
 *      Root node of JSON tree
 *  @param key
 *      Key to look up
 *  @returns A pointer to the relevant node, or NULL if not found
 */
static json_object *qagen_json_dfs_single(json_object *root, const char *key)
/* this is kinda gross */
{
    json_object *res = NULL;

    json_object_object_foreach(root, k, v) {    /* Yeah that won't be confusing later */
        if (json_object_get_type(v) == json_type_object) {
            res = qagen_json_dfs_single(v, key);
        } else {
            if (!strcmp(k, key)) {
                res = v;
            }
        }
        if (res) {
            break;
        }
    }
    return res;
}


/** @brief Search for optional keys
 *  @param pt
 *      Patient context
 *  @param root
 *      JSON root node
 *  @returns Nonzero on error? I don't believe anything can really go wrong
 *      here. pls check it anyway tho
 */
static int qagen_json_opt(struct qagen_patient *pt, json_object *root)
/** After all that work fetching every key in the same DFS, now you're just
 *  being LAZY
 */
{
    static const char *rxdoseky = "total_rx_dose_cgy";
    static const char *nfracky = "number_of_fractions";
    json_object *doseval, *nfracval;

    doseval = qagen_json_dfs_single(root, rxdoseky);
    nfracval = qagen_json_dfs_single(root, nfracky);
    if (doseval && nfracval) {
        errno = 0;
        pt->rxdose_cgy = json_object_get_double(doseval);
        pt->nfrac = json_object_get_int(nfracval);
        if (!errno) {
            pt->hasrx = true;
            qagen_log_printf(QAGEN_LOG_DEBUG, L"Found Rx information: %.2f Gy in %u fractions", pt->rxdose_cgy / 100.0, pt->nfrac);
        }
    }
    return 0;
}


int qagen_json_read(struct qagen_patient *pt, const wchar_t *filename)
{
    json_object *root;
    char *jbuf;
    enum json_tokener_error jerr;
    int res = 1;

    jbuf = qagen_json_load_file(filename);
    if (jbuf) {
        root = json_tokener_parse_verbose(jbuf, &jerr);
        if (root) {
            res = qagen_json_load(pt, root)
               || qagen_json_opt(pt, root);
            json_object_put(root);
        } else {
            qagen_error_raise(QAGEN_ERR_RUNTIME, L"Could not parse JSON file", L"%S", json_tokener_error_desc(jerr));
        }
        qagen_free(jbuf);
    }
    return res;
}
