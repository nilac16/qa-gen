#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "qagen-json.h"
#include "qagen-error.h"
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
    "beamset"
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

static_assert(BUFLEN(pt_keys) == PT_TOK_ISO, "Mismatched patient keys");
static_assert(BUFLEN(field_keys) == PT_N_FLD_TAGS, "Mismatched field keys");


/** @brief Will only return if it succeeds */
static json_object *qagen_json_wstring_node(const wchar_t *str, jmp_buf env)
{
    static const wchar_t *failfmt = L"Failed to convert \"%s\" to UTF-8";
    char cvt[512];
    json_object *res = NULL;
    size_t out = wcstombs(cvt, str, BUFLEN(cvt));
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
    json_object *res = json_object_new_double(x);
    if (!res) {
        qagen_log_printf(QAGEN_LOG_ERROR, L"Failed to create JSON node containing %f", x);
        longjmp(env, 1);
    }
    return res;
}


static json_object *qagen_json_rangeshifter_node(int rsid, jmp_buf env)
{
    json_object *res = json_object_new_int(rsid);
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
    json_object *res = qagen_json_double_node(x, env);
    json_object_set_serializer(res, qagen_json_meterset_serializer, NULL, NULL);
    return res;
}


/** @brief Attaches all of the tokens to root in order */
static void qagen_json_make_tokens(const struct qagen_patient *pt,
                                   json_object                *root,
                                   jmp_buf                     env)
{
    json_object *node;
    for (uint32_t i = 0; i < PT_TOK_ISO; i++) {
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
    json_object *arr = json_object_new_array();
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
    json_object *root = json_object_new_object();
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
    const uint32_t n = qagen_patient_num_beams(pt);
    json_object *arr = json_object_new_array();
    const struct qagen_rtbeam *beam;
    if (arr) {
        for (uint32_t i = 0; i < n; i++) {
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
    json_object *root = json_object_new_object();
    jmp_buf env;
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
    json_object *root = qagen_json_make(pt);
    FILE *fp;
    int res = 0;
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
