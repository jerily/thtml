/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "compiler_c.h"
#include "compiler_tcl.h"
#include "md5.h"
#include <ctype.h>
#include <string.h>
#include <assert.h>

static int count_text_subst = 0;
static int count_var_dict_subst = 0;

static int template_cmd_count = 0;
static int foreach_cmd_count = 0;
static int subcmd_count = 0;
static int op_count = 0;
static int check_count = 0;

int thtml_CCompileQuotedString(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                               const char *name);

int thtml_CCompileCommand(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                          const char *name, int nested, int *compiled_cmd);

void thtml_CGarbageCollection(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr) {

    Tcl_Obj *gc_lists_ptr = Tcl_ObjGetVar2(interp, codearrVar_ptr, Tcl_NewStringObj("gc_lists", -1), TCL_LEAVE_ERR_MSG);
    if (gc_lists_ptr == NULL) {
        return;
    }

    // if empty return
    Tcl_Size gc_lists_length;
    if (TCL_OK != Tcl_ListObjLength(interp, gc_lists_ptr, &gc_lists_length) || gc_lists_length == 0) {
        return;
    }

    // get first element of gc_lists
    Tcl_Obj *gc_list_ptr = NULL;
    if (TCL_OK != Tcl_ListObjIndex(interp, gc_lists_ptr, 0, &gc_list_ptr)) {
        return;
    }

    // get the list of variables to be garbage collected
    // they are interleaved with their type e.g. type varname
    Tcl_Size gc_list_length;
    Tcl_Obj **gc_list;
    if (TCL_OK != Tcl_ListObjGetElements(interp, gc_list_ptr, &gc_list_length, &gc_list)) {
        return;
    }

    Tcl_DStringAppend(ds_ptr, "\n// garbage collection\n", -1);

    for (int i = 0; i < gc_list_length; i += 2) {
        Tcl_Obj *type_ptr = gc_list[i];
        Tcl_Obj *varname_ptr = gc_list[i + 1];

        const char *type = Tcl_GetString(type_ptr);
        if (strcmp(type, "obj") == 0) {
            Tcl_DStringAppend(ds_ptr, "\nTcl_DecrRefCount(", -1);
            Tcl_DStringAppend(ds_ptr, Tcl_GetString(varname_ptr), -1);
            Tcl_DStringAppend(ds_ptr, ");", -1);
        } else if (strcmp(type, "dstring") == 0) {
            Tcl_DStringAppend(ds_ptr, "\nTcl_DStringFree(", -1);
            Tcl_DStringAppend(ds_ptr, Tcl_GetString(varname_ptr), -1);
            Tcl_DStringAppend(ds_ptr, ");", -1);
        } else {
            fprintf(stderr, "Unknown type: %s\n", type);
        }
    }
}

int thtml_CTransformCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "TclTransformCmd\n"));

    CheckArgs(2, 2, 1, "intermediate_code");

    Tcl_Size intermediate_code_length;
    char *intermediate_code = Tcl_GetStringFromObj(objv[1], &intermediate_code_length);

    // special characters that denote the start and end of html Vs code blocks:
    // \x02 - start of text
    // \x03 - end of text

    const char *p = intermediate_code;
    const char *end = intermediate_code + intermediate_code_length;

    Tcl_DString ds;
    Tcl_DStringInit(&ds);

    while (p < end) {
        // make sure "p" points to the start of the first text block, i.e. it is '\x02'
        if (*p != '\x02') {
            fprintf(stderr, "Text block does not start with start-of-text marker ch=%02x\n", *p);
            Tcl_DStringFree(&ds);
            SetResult("Text block does not start with start-of-text marker");
            return TCL_ERROR;
        }

        // skip the first '\x02'
        p++;

        // loop until we reach the end of the text block denoted by '\x03', excluding the last '\x03'
        const char *q = p;
        while (q < end) {
            if (*q == '\x03') {
                Tcl_DStringAppend(&ds, "\nTcl_DStringAppend(__ds_default__, \"", -1);
                thtml_AppendEscaped(p, q, &ds);
                Tcl_DStringAppend(&ds, "\", -1);\n", -1);
                break;
            }
            q++;
        }

        if (q == end) {
            Tcl_DStringFree(&ds);
            SetResult("Text block does not end with end-of-text marker");
            return TCL_ERROR;
        }

        // skip the last '\x03'
        p = q + 1;

        // loop until we reach the end of the code block denoted by '\x02', excluding the last '\x02'
        q = p;
        while (q < end) {
            if (*q == '\x02') {
                Tcl_DStringAppend(&ds, p, q - p);
                break;
            }
            q++;
        }

        // skip the last '\x02'
        p = q;
    }

    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);
    return TCL_OK;
}

int thtml_CCompileExprCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "CCompileExprCmd\n"));

    CheckArgs(4, 4, 1, "codearrVar text name");

    Tcl_Size text_length;
    char *text = Tcl_GetStringFromObj(objv[2], &text_length);

    Tcl_Size name_length;
    char *name = Tcl_GetStringFromObj(objv[3], &name_length);

    Tcl_Parse parse;
    if (TCL_OK != Tcl_ParseExpr(interp, text, text_length, &parse)) {
        Tcl_FreeParse(&parse);
        return TCL_ERROR;
    }

    Tcl_DString ds;
    Tcl_DStringInit(&ds);

    if (TCL_OK != thtml_CCompileExpr(interp, objv[1], &ds, &parse, name)) {
        Tcl_FreeParse(&parse);
        Tcl_DStringFree(&ds);
        return TCL_ERROR;
    }

    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

int thtml_CCompileQuotedStringCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "CCompileQuotedStringCmd\n"));

    CheckArgs(3, 3, 1, "codearrVar text");

    Tcl_Size text_length;
    char *text = Tcl_GetStringFromObj(objv[2], &text_length);

    Tcl_Parse parse;
    if (TCL_OK != Tcl_ParseQuotedString(interp, text, text_length, &parse, 0, NULL)) {
        Tcl_FreeParse(&parse);
        return TCL_ERROR;
    }

    Tcl_DString ds;
    Tcl_DStringInit(&ds);

    if (TCL_OK != thtml_CCompileQuotedString(interp, objv[1], &ds, &parse, "default")) {
        Tcl_FreeParse(&parse);
        Tcl_DStringFree(&ds);
        return TCL_ERROR;
    }

    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

int thtml_CCompileTemplateTextCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "CCompileTemplateTextCmd\n"));

    CheckArgs(3, 3, 1, "codearrVar text");

    Tcl_Size text_length;
    char *text = Tcl_GetStringFromObj(objv[2], &text_length);

    const char *p = text;
    const char *end = text + text_length;

    Tcl_DString text_ds;
    Tcl_DStringInit(&text_ds);
    int count = 0;
    while (p < end) {
        if (*p == '[') {
            count++;
            Tcl_DStringAppend(&text_ds, "[", 1);
        } else if (*p == ']') {
            count--;
            Tcl_DStringAppend(&text_ds, "]", 1);
        } else if (p > text && p < end - 1 && *p == '"' && count == 0) {
            Tcl_DStringAppend(&text_ds, "\\\"", 2);
        } else if (p > text && p < end - 1 && *p == '\\' && count == 0) {
            if (p + 1 < end - 1 && *(p + 1) == '[') {
                Tcl_DStringAppend(&text_ds, "\\[", 2);
                p += 2;
                continue;
            } else if (p + 1 < end - 1 && *(p + 1) == ']') {
                Tcl_DStringAppend(&text_ds, "\\]", 2);
                p += 2;
                continue;
            } else if (p + 1 < end - 1 && *(p + 1) == '"') {
                // do nothing, we escape double quotes when we see them
            } else {
                Tcl_DStringAppend(&text_ds, "\\", 1);
            }
        } else {
            Tcl_DStringAppend(&text_ds, p, 1);
        }
        p++;
    }

    Tcl_Parse parse;
    if (TCL_OK !=
        Tcl_ParseQuotedString(interp, Tcl_DStringValue(&text_ds), Tcl_DStringLength(&text_ds), &parse, 0, NULL)) {
        Tcl_FreeParse(&parse);
        return TCL_ERROR;
    }

    Tcl_DString ds;
    Tcl_DStringInit(&ds);

    if (TCL_OK != thtml_CCompileTemplateText(interp, objv[1], &ds, &parse)) {
        Tcl_FreeParse(&parse);
        Tcl_DStringFree(&ds);
        return TCL_ERROR;
    }

    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

int thtml_CCompileScriptCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "CCompileScriptCmd\n"));

    CheckArgs(4, 4, 1, "codearrVar text name");

    Tcl_Size text_length;
    const char *text = Tcl_GetStringFromObj(objv[2], &text_length);

    Tcl_Size name_length;
    const char *name = Tcl_GetStringFromObj(objv[3], &name_length);

    Tcl_DString ds;
    Tcl_DStringInit(&ds);

    Tcl_Parse parse;
    if (TCL_OK != Tcl_ParseCommand(interp, text, text_length, 0, &parse)) {
        Tcl_FreeParse(&parse);
        return TCL_ERROR;
    }

    Tcl_DStringAppend(&ds, "\nTcl_DString __ds_", -1);
    Tcl_DStringAppend(&ds, name, name_length);
    Tcl_DStringAppend(&ds, "_base__;", -1);
    Tcl_DStringAppend(&ds, "\nTcl_DString *__ds_", -1);
    Tcl_DStringAppend(&ds, name, name_length);
    Tcl_DStringAppend(&ds, "__ = &__ds_", -1);
    Tcl_DStringAppend(&ds, name, name_length);
    Tcl_DStringAppend(&ds, "_base__;", -1);
    Tcl_DStringAppend(&ds, "\nTcl_DStringInit(__ds_", -1);
    Tcl_DStringAppend(&ds, name, name_length);
    Tcl_DStringAppend(&ds, "__);", -1);

    Tcl_DString script_ds;
    Tcl_DStringInit(&script_ds);

    int compiled_cmd = 0;
    if (TCL_OK != thtml_CCompileCommand(interp, objv[1], &script_ds, &parse, name, 0, &compiled_cmd)) {
        Tcl_FreeParse(&parse);
        Tcl_DStringFree(&ds);
        Tcl_DStringFree(&script_ds);
        return TCL_ERROR;
    }

    if (!compiled_cmd) {

        // Tcl_DStringAppend(__ds_val1__, "::thtml::runtime::tcl::evaluate_script", -1);
        Tcl_DStringAppend(&ds, "\nTcl_DStringAppend(__ds_", -1);
        Tcl_DStringAppend(&ds, name, name_length);
        Tcl_DStringAppend(&ds, "__, \"::thtml::runtime::tcl::evaluate_script\", -1);", -1);

        // Tcl_DStringStartSublist(__ds_val1__);
        Tcl_DStringAppend(&ds, "\nTcl_DStringStartSublist(__ds_", -1);
        Tcl_DStringAppend(&ds, name, name_length);
        Tcl_DStringAppend(&ds, "__);", -1);

        Tcl_DStringAppend(&ds, Tcl_DStringValue(&script_ds), Tcl_DStringLength(&script_ds));

        // Tcl_DStringStartSublist(__ds_val1__);
        Tcl_DStringAppend(&ds, "\nTcl_DStringEndSublist(__ds_", -1);
        Tcl_DStringAppend(&ds, name, name_length);
        Tcl_DStringAppend(&ds, "__);", -1);

        // Tcl_Obj *__val${val_num}__ = Tcl_DStringToObj(__ds_val1__);
        Tcl_DStringAppend(&ds, "\nTcl_Obj *__", -1);
        Tcl_DStringAppend(&ds, name, name_length);
        Tcl_DStringAppend(&ds, "__ = Tcl_DStringToObj(__ds_", -1);
        Tcl_DStringAppend(&ds, name, name_length);
        Tcl_DStringAppend(&ds, "__);", -1);

        // Tcl_IncrRefCount(__val${val_num}__);
        Tcl_DStringAppend(&ds, "\nTcl_IncrRefCount(__", -1);
        Tcl_DStringAppend(&ds, name, name_length);
        Tcl_DStringAppend(&ds, "__);\n", -1);

        // fprintf(stderr, "%s\n", Tcl_GetString(__val${val_num}__));
//        Tcl_DStringAppend(&ds, "\nfprintf(stderr, \"__val${val_num}__ = %s\\n\", Tcl_GetString(__", -1);
//        Tcl_DStringAppend(&ds, name, name_length);
//        Tcl_DStringAppend(&ds, "__));\n", -1);

        // if (TCL_OK != Tcl_EvalObjEx(__interp__, __val${val_num}__, TCL_EVAL_DIRECT)) {return TCL_ERROR;}
        Tcl_DStringAppend(&ds, "\nif (TCL_OK != Tcl_EvalObjEx(__interp__, __", -1);
        Tcl_DStringAppend(&ds, name, name_length);
        Tcl_DStringAppend(&ds, "__, TCL_EVAL_DIRECT)) {", -1);
        // Tcl_DecrRefCount(__val${val_num}__);
        thtml_CGarbageCollection(interp, objv[1], &ds);
        Tcl_DStringAppend(&ds, "\nreturn TCL_ERROR;\n}", -1);

        // Tcl_DecrRefCount(__val${val_num}__);
        Tcl_DStringAppend(&ds, "\nTcl_DecrRefCount(__", -1);
        Tcl_DStringAppend(&ds, name, name_length);
        Tcl_DStringAppend(&ds, "__);\n", -1);

        // Tcl_DStringFree(__ds_val1__);
        Tcl_DStringAppend(&ds, "\nTcl_DStringFree(__ds_", -1);
        Tcl_DStringAppend(&ds, name, name_length);
        Tcl_DStringAppend(&ds, "__);", -1);


    } else {
        Tcl_DStringAppend(&ds, Tcl_DStringValue(&script_ds), Tcl_DStringLength(&script_ds));
    }

    Tcl_DStringAppend(&ds, "\nTcl_DStringFree(&__ds_", -1);
    Tcl_DStringAppend(&ds, name, name_length);
    Tcl_DStringAppend(&ds, "__);", -1);

    Tcl_FreeParse(&parse);

    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);
    Tcl_DStringFree(&script_ds);
    return TCL_OK;
}

#define THTML_IN_EVAL 1
#define THTML_NUMBER 1 << 1
#define THTML_STRING 1 << 2
#define THTML_BOOL 1 << 3

static int
thtml_CAppendExpr_Token(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                        Tcl_Size i, const char *name, Tcl_DString *expr_ds_ptr, Tcl_DString *after_ds_ptr, int flags);

static int thtml_CAppendCheck_Bool(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, const char *varname) {
    Tcl_DStringAppend(ds_ptr, "\n// check boolean: ", -1);
    Tcl_DStringAppend(ds_ptr, varname, -1);

    // int __a_val__ = 0;
    Tcl_DStringAppend(ds_ptr, "\nint __", -1);
    Tcl_DStringAppend(ds_ptr, varname, -1);
    Tcl_DStringAppend(ds_ptr, "_val", -1);
    Tcl_DStringAppend(ds_ptr, "__ = 0;", -1);

    // if (TCL_OK != Tcl_GetBooleanFromObj(__interp__, a, &__a_val__)) { return TCL_ERROR; }
    Tcl_DStringAppend(ds_ptr, "\nif (TCL_OK != Tcl_GetBooleanFromObj(__interp__, ", -1);
    Tcl_DStringAppend(ds_ptr, varname, -1);
    Tcl_DStringAppend(ds_ptr, ", &__", -1);
    Tcl_DStringAppend(ds_ptr, varname, -1);
    Tcl_DStringAppend(ds_ptr, "_val", -1);
    Tcl_DStringAppend(ds_ptr, "__)) {", -1);
    thtml_CGarbageCollection(interp, codearrVar_ptr, ds_ptr);
    Tcl_DStringAppend(ds_ptr, "\nSetResult(\"invalid in boolean check\");\nreturn TCL_ERROR; }", -1);

    return TCL_OK;
}

static int thtml_CAppendCheck_Number(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, const char *varname) {

    check_count++;

    char check_count_str[12];
    snprintf(check_count_str, 12, "%d", check_count);

    Tcl_DStringAppend(ds_ptr, "\n// check number: ", -1);
    Tcl_DStringAppend(ds_ptr, varname, -1);

    // void *__a_val__ = NULL;
    Tcl_DStringAppend(ds_ptr, "\nvoid *__", -1);
    Tcl_DStringAppend(ds_ptr, varname, -1);
    Tcl_DStringAppend(ds_ptr, "_val", -1);
    Tcl_DStringAppend(ds_ptr, check_count_str, -1);
    Tcl_DStringAppend(ds_ptr, "__ = NULL;", -1);

    // int __a_type__ = TCL_NUMBER_NAN;
    Tcl_DStringAppend(ds_ptr, "\nint __", -1);
    Tcl_DStringAppend(ds_ptr, varname, -1);
    Tcl_DStringAppend(ds_ptr, "_type", -1);
    Tcl_DStringAppend(ds_ptr, check_count_str, -1);
    Tcl_DStringAppend(ds_ptr, "__ = TCL_NUMBER_NAN;", -1);

    // if (TCL_OK != Tcl_GetNumberFromObj(__interp__, a, &__a_val__, &__a_type__) || __a_type__ == TCL_NUMBER_NAN) { return TCL_ERROR; }
    Tcl_DStringAppend(ds_ptr, "\nif (TCL_OK != Tcl_GetNumberFromObj(__interp__, ", -1);
    Tcl_DStringAppend(ds_ptr, varname, -1);
    Tcl_DStringAppend(ds_ptr, ", &__", -1);
    Tcl_DStringAppend(ds_ptr, varname, -1);
    Tcl_DStringAppend(ds_ptr, "_val", -1);
    Tcl_DStringAppend(ds_ptr, check_count_str, -1);
    Tcl_DStringAppend(ds_ptr, "__, &__", -1);
    Tcl_DStringAppend(ds_ptr, varname, -1);
    Tcl_DStringAppend(ds_ptr, "_type", -1);
    Tcl_DStringAppend(ds_ptr, check_count_str, -1);
    Tcl_DStringAppend(ds_ptr, "__) || __", -1);
    Tcl_DStringAppend(ds_ptr, varname, -1);
    Tcl_DStringAppend(ds_ptr, "_type", -1);
    Tcl_DStringAppend(ds_ptr, check_count_str, -1);
    Tcl_DStringAppend(ds_ptr, "__ == TCL_NUMBER_NAN) {", -1);
    thtml_CGarbageCollection(interp, codearrVar_ptr, ds_ptr);
    Tcl_DStringAppend(ds_ptr, "\nSetResult(\"invalid in number check\");\nreturn TCL_ERROR; }", -1);
    return TCL_OK;
}

static int thtml_CAppendVariable_Simple(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, const char *varname_first_part,
                                        Tcl_Size varname_first_part_length, const char *name, Tcl_DString *expr_ds_ptr,
                                        int flags) {
    if (expr_ds_ptr == NULL) {
        if (0 && (flags & THTML_IN_EVAL)) {
            // Tcl_Obj *__script1_var_x__ = Tcl_NewStringObj("x", -1);
            Tcl_DStringAppend(ds_ptr, "\nTcl_Obj *__", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "_var_", -1);
            Tcl_DStringAppend(ds_ptr, varname_first_part, varname_first_part_length);
            Tcl_DStringAppend(ds_ptr, "__ = Tcl_NewStringObj(\"", -1);
            Tcl_DStringAppend(ds_ptr, varname_first_part, varname_first_part_length);
            Tcl_DStringAppend(ds_ptr, "\", -1);", -1);
            // Tcl_IncrRefCount(__script1_var_x__);
            Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(__", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "_var_", -1);
            Tcl_DStringAppend(ds_ptr, varname_first_part, varname_first_part_length);
            Tcl_DStringAppend(ds_ptr, "__);", -1);
            // if (TCL_OK != Tcl_ObjSetVar2(__interp__, __script1_var_x__, NULL, x, TCL_LEAVE_ERR_MSG)) { return TCL_ERROR; }
            Tcl_DStringAppend(ds_ptr, "\nif (TCL_OK != Tcl_ObjSetVar2(__interp__, ", -1);
            Tcl_DStringAppend(ds_ptr, "__", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "_var_", -1);
            Tcl_DStringAppend(ds_ptr, varname_first_part, varname_first_part_length);
            Tcl_DStringAppend(ds_ptr, "__, NULL, ", -1);
            Tcl_DStringAppend(ds_ptr, varname_first_part, varname_first_part_length);
            Tcl_DStringAppend(ds_ptr, ", TCL_LEAVE_ERR_MSG)) {", -1);
            thtml_CGarbageCollection(interp, codearrVar_ptr, ds_ptr);
            Tcl_DStringAppend(ds_ptr, "fprintf(stderr, \"here\\\n\");\nreturn TCL_ERROR; }", -1);
            // Tcl_DecrRefCount(__script1_var_x__);
            Tcl_DStringAppend(ds_ptr, "\nTcl_DecrRefCount(__", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "_var_", -1);
            Tcl_DStringAppend(ds_ptr, varname_first_part, varname_first_part_length);
            Tcl_DStringAppend(ds_ptr, "__);", -1);

            // Tcl_DStringAppend(__ds_default__, "\"$x\"", -1);
            Tcl_DStringAppend(ds_ptr, "\nTcl_DStringAppend(__ds_", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "__, \"$", -1);
            Tcl_DStringAppend(ds_ptr, varname_first_part, varname_first_part_length);
            Tcl_DStringAppend(ds_ptr, "\", -1);", -1);
        } else {
            // Tcl_DStringAppend(__ds_default__, Tcl_GetString(x), -1);
            Tcl_DStringAppend(ds_ptr, "\nTcl_DStringAppend(__ds_", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "__, ", -1);
            Tcl_DStringAppend(ds_ptr, "Tcl_GetString(", -1);
            Tcl_DStringAppend(ds_ptr, varname_first_part, varname_first_part_length);
            Tcl_DStringAppend(ds_ptr, "), -1);", -1);
        }
    } else {
        if (flags & THTML_IN_EVAL) {
            // todo
            Tcl_DStringAppend(expr_ds_ptr, "$", -1);
            Tcl_DStringAppend(expr_ds_ptr, varname_first_part, varname_first_part_length);
        } else {
            if (flags & THTML_NUMBER) {
                thtml_CAppendCheck_Number(interp, codearrVar_ptr, ds_ptr, varname_first_part);
            } else if (flags & THTML_BOOL) {
                thtml_CAppendCheck_Bool(interp, codearrVar_ptr, ds_ptr, varname_first_part);
            }

            Tcl_DStringAppend(expr_ds_ptr, varname_first_part, varname_first_part_length);
        }
    }
    return TCL_OK;
}

static int
thtml_CAppendVariable_Dict(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, const char *varname_first_part,
                           Tcl_Size varname_first_part_length, Tcl_Obj **parts,
                           Tcl_Size num_parts, const char *name, Tcl_DString *expr_ds_ptr, int flags) {

    count_var_dict_subst++;
    char count_var_dict_subst_str[12];
    snprintf(count_var_dict_subst_str, 12, "%d", count_var_dict_subst);

    char varname[64];
    snprintf(varname, 64, "__dict_%s__", count_var_dict_subst_str);

    Tcl_DStringAppend(ds_ptr, "\nTcl_Obj *", -1);
    Tcl_DStringAppend(ds_ptr, varname, -1);
    Tcl_DStringAppend(ds_ptr, " = ", -1);
    Tcl_DStringAppend(ds_ptr, varname_first_part, varname_first_part_length);
    Tcl_DStringAppend(ds_ptr, ";", -1);

    int first = 1;
    for (int i = 0; i < num_parts; i++) {
//        if (first) {
//            first = 0;
//        } else {
//            Tcl_DStringAppend(ds_ptr, " ", 1);
//        }
        Tcl_Obj *part_ptr = parts[i];
        Tcl_Size part_length;
        const char *part = Tcl_GetStringFromObj(part_ptr, &part_length);

        Tcl_DStringAppend(ds_ptr, "\nTcl_Obj *", -1);
        Tcl_DStringAppend(ds_ptr, part, part_length);
        Tcl_DStringAppend(ds_ptr, count_var_dict_subst_str, -1);

        Tcl_DStringAppend(ds_ptr, ";", -1);
        Tcl_DStringAppend(ds_ptr, "\nTcl_Obj *__", -1);
        Tcl_DStringAppend(ds_ptr, part, part_length);
        Tcl_DStringAppend(ds_ptr, count_var_dict_subst_str, -1);
        Tcl_DStringAppend(ds_ptr, "_key_ptr__ = Tcl_NewStringObj(\"", -1);
        Tcl_DStringAppend(ds_ptr, part, part_length);
        Tcl_DStringAppend(ds_ptr, "\", -1);", -1);
        Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(__", -1);
        Tcl_DStringAppend(ds_ptr, part, part_length);
        Tcl_DStringAppend(ds_ptr, count_var_dict_subst_str, -1);
        Tcl_DStringAppend(ds_ptr, "_key_ptr__);", -1);
        Tcl_DStringAppend(ds_ptr, "\nif (TCL_OK != Tcl_DictObjGet(__interp__, ", -1);
        Tcl_DStringAppend(ds_ptr, "", -1);
        Tcl_DStringAppend(ds_ptr, varname, -1);
        Tcl_DStringAppend(ds_ptr, ", __", -1);
        Tcl_DStringAppend(ds_ptr, part, part_length);
        Tcl_DStringAppend(ds_ptr, count_var_dict_subst_str, -1);
        Tcl_DStringAppend(ds_ptr, "_key_ptr__, &", -1);
        Tcl_DStringAppend(ds_ptr, part, part_length);
        Tcl_DStringAppend(ds_ptr, count_var_dict_subst_str, -1);
        Tcl_DStringAppend(ds_ptr, ") || !", -1);
        Tcl_DStringAppend(ds_ptr, part, part_length);
        Tcl_DStringAppend(ds_ptr, count_var_dict_subst_str, -1);
        Tcl_DStringAppend(ds_ptr, ") {", -1);
        thtml_CGarbageCollection(interp, codearrVar_ptr, ds_ptr);
        Tcl_DStringAppend(ds_ptr, "\nSetResult(\"dict obj get failed: ", -1);
        Tcl_DStringAppend(ds_ptr, part, part_length);
        Tcl_DStringAppend(ds_ptr, "\");\nreturn TCL_ERROR;\n}", -1);
        Tcl_DStringAppend(ds_ptr, "\nTcl_DecrRefCount(__", -1);
        Tcl_DStringAppend(ds_ptr, part, part_length);
        Tcl_DStringAppend(ds_ptr, count_var_dict_subst_str, -1);
        Tcl_DStringAppend(ds_ptr, "_key_ptr__);", -1);


//         fprintf(stderr, "dict: %s\n", Tcl_GetString(__dict1__));
//        Tcl_DStringAppend(ds_ptr, "\nfprintf(stderr, \"dict: %s\\n\", Tcl_GetString(__dict_", -1);
//        Tcl_DStringAppend(ds_ptr, count_var_dict_subst_str, -1);
//        Tcl_DStringAppend(ds_ptr, "__));\n", -1);

        Tcl_DStringAppend(ds_ptr, "\n", -1);
        Tcl_DStringAppend(ds_ptr, varname, -1);
        Tcl_DStringAppend(ds_ptr, " = ", -1);
        Tcl_DStringAppend(ds_ptr, part, part_length);
        Tcl_DStringAppend(ds_ptr, count_var_dict_subst_str, -1);
        Tcl_DStringAppend(ds_ptr, ";", -1);

    }

    if (flags & THTML_NUMBER) {
        thtml_CAppendCheck_Number(interp, codearrVar_ptr, ds_ptr, varname);
    } else if (flags & THTML_BOOL) {
        thtml_CAppendCheck_Bool(interp, codearrVar_ptr, ds_ptr, varname);
    }

    if (expr_ds_ptr == NULL) {
        // Tcl_DStringAppend(__ds_wt3__, Tcl_GetString(__dict_2__), -1);
        Tcl_DStringAppend(ds_ptr, "\nTcl_DStringAppend(__ds_", -1);
        Tcl_DStringAppend(ds_ptr, name, -1);
        Tcl_DStringAppend(ds_ptr, "__, Tcl_GetString(__dict_", -1);
        Tcl_DStringAppend(ds_ptr, count_var_dict_subst_str, -1);
        Tcl_DStringAppend(ds_ptr, "__), -1);\n", -1);

        // fprintf(stderr, "%s\n", Tcl_GetString(__dict1__));
//        Tcl_DStringAppend(ds_ptr, "\nfprintf(stderr, \"%s\\n\", Tcl_GetString(__dict_", -1);
//        Tcl_DStringAppend(ds_ptr, count_var_dict_subst_str, -1);
//        Tcl_DStringAppend(ds_ptr, "__));\n", -1);

    } else {
        Tcl_DStringAppend(expr_ds_ptr, "__dict_", -1);
        Tcl_DStringAppend(expr_ds_ptr, count_var_dict_subst_str, -1);
        Tcl_DStringAppend(expr_ds_ptr, "__", -1);
    }

    return TCL_OK;
}

static int
thtml_CAppendVariable(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                      Tcl_Size i, const char *name, Tcl_DString *cmd_ds_ptr, int flags) {
    Tcl_Token *token = &parse_ptr->tokenPtr[i];
    Tcl_Size numComponents = token->numComponents;
    if (numComponents == 1) {
        Tcl_Token *text_token = &parse_ptr->tokenPtr[i + 1];

        // split text_token->start into parts by "."
        const char *p = text_token->start;
        const char *end = text_token->start + text_token->size;

        Tcl_Obj *parts_ptr = Tcl_NewListObj(0, NULL);
        Tcl_IncrRefCount(parts_ptr);
        while (p < end) {
            const char *start = p;
            while (p < end && *p != '.') {
                p++;
            }
            Tcl_Obj *part_ptr = Tcl_NewStringObj(start, p - start);
            if (TCL_OK != Tcl_ListObjAppendElement(interp, parts_ptr, part_ptr)) {
                Tcl_DecrRefCount(parts_ptr);
                return TCL_ERROR;
            }
            if (p < end) {
                p++;
            }
        }

        // figure out whether it is a simple var or a dict reference
        Tcl_Size num_parts;
        Tcl_Obj **parts;
        if (TCL_OK != Tcl_ListObjGetElements(interp, parts_ptr, &num_parts, &parts)) {
            Tcl_DecrRefCount(parts_ptr);
            return TCL_ERROR;
        }

        Tcl_Obj *blocks_key_ptr = Tcl_NewStringObj("blocks", 6);
        Tcl_IncrRefCount(blocks_key_ptr);
        Tcl_Obj *blocks_list_ptr = Tcl_ObjGetVar2(interp, codearrVar_ptr, blocks_key_ptr, TCL_LEAVE_ERR_MSG);
        Tcl_DecrRefCount(blocks_key_ptr);

        if (blocks_list_ptr == NULL) {
            Tcl_DecrRefCount(parts_ptr);
            SetResult("error getting blocks from codearr");
            return TCL_ERROR;
        }

        // iterate "blocks_list_ptr"
        Tcl_Size num_blocks;
        Tcl_Obj **blocks;
        if (TCL_OK != Tcl_ListObjGetElements(interp, blocks_list_ptr, &num_blocks, &blocks)) {
            Tcl_DecrRefCount(parts_ptr);
            return TCL_ERROR;
        }

        // for each block, get the "variables" list from the dictionary
        for (int j = 0; j < num_blocks; j++) {
            Tcl_Obj *block_ptr = blocks[j];

//            fprintf(stderr, "block: %s\n", Tcl_GetString(block_ptr));

            Tcl_Obj *variables_key_ptr = Tcl_NewStringObj("varnames", -1);
            Tcl_IncrRefCount(variables_key_ptr);
            Tcl_Obj *variables_ptr = NULL;
            if (TCL_OK != Tcl_DictObjGet(interp, block_ptr, variables_key_ptr, &variables_ptr)) {
                Tcl_DecrRefCount(variables_key_ptr);
                Tcl_DecrRefCount(parts_ptr);
                return TCL_ERROR;
            }
            Tcl_DecrRefCount(variables_key_ptr);

            if (variables_ptr == NULL) {
//                fprintf(stderr, "no varnames\n");
                continue;
            }

            // get the first part of the variable name
            Tcl_Obj *varname_first_part_ptr = parts[0];

            // for each "block_varname_ptr" in "variables_ptr", compare it with "varname_first_part_ptr"
            Tcl_Size num_variables;
            Tcl_Obj **variables;
            if (TCL_OK != Tcl_ListObjGetElements(interp, variables_ptr, &num_variables, &variables)) {
                Tcl_DecrRefCount(parts_ptr);
                return TCL_ERROR;
            }

            Tcl_Size varname_first_part_length;
            const char *varname_first_part = Tcl_GetStringFromObj(varname_first_part_ptr, &varname_first_part_length);
            for (int k = 0; k < num_variables; k++) {
                Tcl_Obj *block_varname_ptr = variables[k];
                Tcl_Size block_varname_length;
                char *block_varname = Tcl_GetStringFromObj(block_varname_ptr, &block_varname_length);

//                fprintf(stderr, "block_varname: %s\n", block_varname);
//                fprintf(stderr, "varname_first_part: %s\n", varname_first_part);
//                fprintf(stderr, "----\n");

                if (block_varname_length == varname_first_part_length &&
                    0 == strncmp(block_varname, varname_first_part, block_varname_length)) {
                    // found a match
                    if (num_parts == 1) {
                        if (TCL_OK != thtml_CAppendVariable_Simple(interp, codearrVar_ptr, ds_ptr, varname_first_part,
                                                                   varname_first_part_length, name, cmd_ds_ptr,
                                                                   flags)) {
                            Tcl_DecrRefCount(parts_ptr);
                            return TCL_ERROR;
                        }
                    } else {
                        if (TCL_OK !=
                            thtml_CAppendVariable_Dict(interp, codearrVar_ptr, ds_ptr, varname_first_part,
                                                       varname_first_part_length, &parts[1], num_parts - 1, name,
                                                       cmd_ds_ptr, flags)) {
                            Tcl_DecrRefCount(parts_ptr);
                            return TCL_ERROR;
                        }
                    }
                    Tcl_DecrRefCount(parts_ptr);
                    return TCL_OK;
                }
            }

            // check if we have a "stop" directive in block
            Tcl_Obj *stop_key_ptr = Tcl_NewStringObj("stop", -1);
            Tcl_IncrRefCount(stop_key_ptr);
            Tcl_Obj *stop_ptr = NULL;
            if (TCL_OK != Tcl_DictObjGet(interp, block_ptr, stop_key_ptr, &stop_ptr)) {
                Tcl_DecrRefCount(stop_key_ptr);
                Tcl_DecrRefCount(parts_ptr);
                return TCL_ERROR;
            }
            Tcl_DecrRefCount(stop_key_ptr);

            if (stop_ptr != NULL) {
                break;
            }
        }

        if (TCL_OK !=
            thtml_CAppendVariable_Dict(interp, codearrVar_ptr, ds_ptr, "__data__", 8, parts, num_parts, name, cmd_ds_ptr, flags)) {
            Tcl_DecrRefCount(parts_ptr);
            return TCL_ERROR;
        }

        Tcl_DecrRefCount(parts_ptr);
    } else {
        SetResult("error parsing expression: array variables not supported");
        return TCL_ERROR;
    }
    return TCL_OK;
}

const char *thtml_GetOperandType(Tcl_Token *token) {
    switch (token->type) {
        case TCL_TOKEN_TEXT:
            return "text";
        case TCL_TOKEN_VARIABLE:
            return "variable";
        case TCL_TOKEN_COMMAND:
            return "command";
        case TCL_TOKEN_EXPAND_WORD:
            return "expandword";
        case TCL_TOKEN_WORD:
            return "word";
        case TCL_TOKEN_BS:
            return "bs";
        case TCL_TOKEN_SUB_EXPR: {
            // check if it is a simple expression e.g. text or variable
            Tcl_Token *sub_token = &token[1];
            switch (sub_token->type) {
                case TCL_TOKEN_TEXT:
                    return "text";
                case TCL_TOKEN_VARIABLE:
                    return "variable";
                default:
                    return "subexpr";
            }
        }
        case TCL_TOKEN_OPERATOR:
            return "operator";
        default:
            return "unknown";
    }
}

static const char *INSTR[] = {
        ['+'] = "add",
        ['-'] = "sub",
        ['*'] = "mult",
        ['/'] = "div",
        ['%'] = "mod",
        ['<'] = "lt",
        ['>'] = "gt",
        ['&'] = "bitand",
        ['^'] = "bitxor",
        ['|'] = "bitor",
        ['~'] = "bitnot",
        ['?'] = "ternary",
        ['!'] = "not",

        [('*' << 8) + '*'] = "expon",
        [('<' << 8) + '<'] = "lshift",
        [('>' << 8) + '>'] = "rshift",
        [('=' << 8) + '='] = "eq",
        [('!' << 8) + '='] = "ne",
        [('e' << 8) + 'q'] = "streq",
        [('n' << 8) + 'e'] = "strneq",
        [('i' << 8) + 'n'] = "in",
        [('n' << 8) + 'i'] = "ni",
        [('<' << 8) + '='] = "lte",
        [('>' << 8) + '='] = "gte",
        [('&' << 8) + '&'] = "and",
        [('|' << 8) + '|'] = "or"

};

static int
thtml_CAppendExpr_Operator(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                           Tcl_Size i, const char *name, Tcl_DString *cmd_ds_ptr, Tcl_DString *after_ds_ptr,
                           int flags) {
    Tcl_Token *subexpr_token = &parse_ptr->tokenPtr[i];
    Tcl_Token *operator_token = &parse_ptr->tokenPtr[i + 1];
    Tcl_Size operands_offset = i + 2;

    // The numComponents field for a TCL_TOKEN_OPERATOR token is always 0
    // So, we get numComponents from the TCL_TOKEN_SUB_EXPR token that precedes it
    Tcl_Size num_components = subexpr_token->numComponents;

    int num_operands = 0;
    Tcl_Size j = 0;
    while (j < num_components - 2) {
        Tcl_Token *operand_token = &parse_ptr->tokenPtr[operands_offset + j];
        if (operand_token->type == TCL_TOKEN_SUB_EXPR) {
            // fprintf(stderr, ">>> subexpr %d\n", operand_token->numComponents);
            num_operands++;
            j += 1 + operand_token->numComponents;
            continue;
        }
        // fprintf(stderr, "num_operands: %d j: %d num_components: %d\n", num_operands, j, num_components);
        // the following should never happen
        SetResult("error parsing expression: not enough operands");
        return TCL_ERROR;
    }


    if (operator_token->size == 1) {
        char ch = operator_token->start[0];
        if (num_operands == 1) {
            if (ch == '!') {
                // logical not, one operand
                op_count++;
                char op_count_str[12];
                snprintf(op_count_str, 12, "%d", op_count);

                char varname[64];
                snprintf(varname, 64, "__%s_op%s__", name, op_count_str);

                Tcl_DString operator_ds;
                Tcl_DStringInit(&operator_ds);

                Tcl_DStringAppend(&operator_ds, "\nTcl_Obj *", -1);
                Tcl_DStringAppend(&operator_ds, varname, -1);
                Tcl_DStringAppend(&operator_ds, " = ", -1);
                Tcl_DStringAppend(&operator_ds, " __thtml_", -1);
                Tcl_DStringAppend(&operator_ds, INSTR[operator_token->start[0]], -1);
                Tcl_DStringAppend(&operator_ds, "__(", -1);

                if (TCL_OK != thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, operands_offset, name,
                                                      &operator_ds, after_ds_ptr, THTML_BOOL)) {
                    Tcl_DStringFree(&operator_ds);
                    return TCL_ERROR;
                }
                Tcl_DStringAppend(&operator_ds, ");", -1);
                Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&operator_ds), Tcl_DStringLength(&operator_ds));
                Tcl_DStringFree(&operator_ds);

                Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(", -1);
                Tcl_DStringAppend(ds_ptr, varname, -1);
                Tcl_DStringAppend(ds_ptr, ");", -1);

                Tcl_DStringAppend(cmd_ds_ptr, varname, -1);

                Tcl_DStringAppend(after_ds_ptr, "\nTcl_DecrRefCount(", -1);
                Tcl_DStringAppend(after_ds_ptr, varname, -1);
                Tcl_DStringAppend(after_ds_ptr, ");", -1);

            } else if (ch == '-') {
                // uminus, one operand

                op_count++;
                char op_count_str[12];
                snprintf(op_count_str, 12, "%d", op_count);

                char varname[64];
                snprintf(varname, 64, "__%s_op%s__", name, op_count_str);

                Tcl_DString operator_ds;
                Tcl_DStringInit(&operator_ds);

                Tcl_DStringAppend(&operator_ds, "\nTcl_Obj *", -1);
                Tcl_DStringAppend(&operator_ds, varname, -1);
                Tcl_DStringAppend(&operator_ds, " = ", -1);
                Tcl_DStringAppend(&operator_ds, " __thtml_uminus__(", -1);

                if (TCL_OK != thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, operands_offset, name,
                                                      &operator_ds, after_ds_ptr, THTML_BOOL)) {
                    Tcl_DStringFree(&operator_ds);
                    return TCL_ERROR;
                }
                Tcl_DStringAppend(&operator_ds, ");", -1);
                Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&operator_ds), Tcl_DStringLength(&operator_ds));
                Tcl_DStringFree(&operator_ds);

                Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(", -1);
                Tcl_DStringAppend(ds_ptr, varname, -1);
                Tcl_DStringAppend(ds_ptr, ");", -1);

                Tcl_DStringAppend(cmd_ds_ptr, varname, -1);

                Tcl_DStringAppend(after_ds_ptr, "\nTcl_DecrRefCount(", -1);
                Tcl_DStringAppend(after_ds_ptr, varname, -1);
                Tcl_DStringAppend(after_ds_ptr, ");", -1);
            } else if (ch == '~') {
                // bitnot, one operand

                op_count++;
                char op_count_str[12];
                snprintf(op_count_str, 12, "%d", op_count);

                char varname[64];
                snprintf(varname, 64, "__%s_op%s__", name, op_count_str);

                Tcl_DString operator_ds;
                Tcl_DStringInit(&operator_ds);

                Tcl_DStringAppend(&operator_ds, "\nTcl_Obj *", -1);
                Tcl_DStringAppend(&operator_ds, varname, -1);
                Tcl_DStringAppend(&operator_ds, " = ", -1);
                Tcl_DStringAppend(&operator_ds, " __thtml_bitnot__(", -1);

                if (TCL_OK != thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, operands_offset, name,
                                                      &operator_ds, after_ds_ptr, THTML_BOOL)) {
                    Tcl_DStringFree(&operator_ds);
                    return TCL_ERROR;
                }
                Tcl_DStringAppend(&operator_ds, ");", -1);
                Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&operator_ds), Tcl_DStringLength(&operator_ds));
                Tcl_DStringFree(&operator_ds);

                Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(", -1);
                Tcl_DStringAppend(ds_ptr, varname, -1);
                Tcl_DStringAppend(ds_ptr, ");", -1);

                Tcl_DStringAppend(cmd_ds_ptr, varname, -1);

                Tcl_DStringAppend(after_ds_ptr, "\nTcl_DecrRefCount(", -1);
                Tcl_DStringAppend(after_ds_ptr, varname, -1);
                Tcl_DStringAppend(after_ds_ptr, ");", -1);
            }
        } else if (num_operands == 2 &&
                   (ch == '<' || ch == '>')) {

            // two operands
            op_count++;
            char op_count_str[12];
            snprintf(op_count_str, 12, "%d", op_count);

            char varname[64];
            snprintf(varname, 64, "__%s_op%s__", name, op_count_str);

            Tcl_DString operator_ds;
            Tcl_DStringInit(&operator_ds);

            Tcl_DStringAppend(&operator_ds, "\nTcl_Obj *", -1);
            Tcl_DStringAppend(&operator_ds, varname, -1);
            Tcl_DStringAppend(&operator_ds, " = ", -1);
            Tcl_DStringAppend(&operator_ds, " __thtml_", -11);
            Tcl_DStringAppend(&operator_ds, INSTR[operator_token->start[0]], -1);
            Tcl_DStringAppend(&operator_ds, "__(", -1);

            Tcl_Token *first_operand = &parse_ptr->tokenPtr[operands_offset];

            // fprintf(stderr, "first_operand of op: %c has numComponents: %d\n", ch, first_operand->numComponents);
            if (TCL_OK !=
                thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, operands_offset, name, &operator_ds,
                                        after_ds_ptr, THTML_NUMBER)) {
                Tcl_DStringFree(&operator_ds);
                return TCL_ERROR;
            }

            Tcl_DStringAppend(&operator_ds, ", ", -1);

            if (TCL_OK != thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr,
                                                  operands_offset + first_operand->numComponents + 1, name,
                                                  &operator_ds, after_ds_ptr, THTML_NUMBER)) {
                Tcl_DStringFree(&operator_ds);
                return TCL_ERROR;
            }

            Tcl_DStringAppend(&operator_ds, ");", -1);

            // check for divided_by_zero, exponent_of_zero, general_arithmetic_error, and out_of_memory
            // if ( __cmd29_expr_op30__ < 0 && __cmd29_expr_op30__ > -5 ) { return TCL_ERROR; }
            Tcl_DStringAppend(&operator_ds, "\nif ( ", -1);
            Tcl_DStringAppend(&operator_ds, varname, -1);
            Tcl_DStringAppend(&operator_ds, " < (Tcl_Obj *) 0 ", -1);
            Tcl_DStringAppend(&operator_ds, " && ", -1);
            Tcl_DStringAppend(&operator_ds, varname, -1);
            Tcl_DStringAppend(&operator_ds, " > (Tcl_Obj *) -5 ) {", -1);
            thtml_CGarbageCollection(interp, codearrVar_ptr, &operator_ds);
            Tcl_DStringAppend(&operator_ds, "SetResult(\"math error in CAppendExpr_Operator\");  return TCL_ERROR; }", -1);

            Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&operator_ds), Tcl_DStringLength(&operator_ds));
            Tcl_DStringFree(&operator_ds);

            Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(", -1);
            Tcl_DStringAppend(ds_ptr, varname, -1);
            Tcl_DStringAppend(ds_ptr, ");", -1);

            Tcl_DStringAppend(cmd_ds_ptr, varname, -1);

            Tcl_DStringAppend(after_ds_ptr, "\nTcl_DecrRefCount(", -1);
            Tcl_DStringAppend(after_ds_ptr, varname, -1);
            Tcl_DStringAppend(after_ds_ptr, ");", -1);

        } else if (num_operands == 2 &&
                   (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' || ch == '^' || ch == '&' ||
                    ch == '|' ||
                    ch == '~')) {

            // binary math op, two operands
            op_count++;
            char op_count_str[12];
            snprintf(op_count_str, 12, "%d", op_count);

            char varname[64];
            snprintf(varname, 64, "__%s_op%s__", name, op_count_str);

            Tcl_DString operator_ds;
            Tcl_DStringInit(&operator_ds);

            Tcl_DStringAppend(&operator_ds, "\nTcl_Obj *", -1);
            Tcl_DStringAppend(&operator_ds, varname, -1);
            Tcl_DStringAppend(&operator_ds, " = ", -1);
            Tcl_DStringAppend(&operator_ds, " __thtml_", -11);
            Tcl_DStringAppend(&operator_ds, INSTR[operator_token->start[0]], -1);
            Tcl_DStringAppend(&operator_ds, "__(__interp__, ", -1);

            Tcl_Token *first_operand = &parse_ptr->tokenPtr[operands_offset];

            // fprintf(stderr, "first_operand of op: %c has numComponents: %d\n", ch, first_operand->numComponents);
            if (TCL_OK !=
                thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, operands_offset, name, &operator_ds,
                                        after_ds_ptr, THTML_NUMBER)) {
                Tcl_DStringFree(&operator_ds);
                return TCL_ERROR;
            }

            Tcl_DStringAppend(&operator_ds, ", ", -1);

            if (TCL_OK != thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr,
                                                  operands_offset + first_operand->numComponents + 1, name,
                                                  &operator_ds, after_ds_ptr, THTML_NUMBER)) {
                Tcl_DStringFree(&operator_ds);
                return TCL_ERROR;
            }

            Tcl_DStringAppend(&operator_ds, ");", -1);

            // check for divided_by_zero, exponent_of_zero, general_arithmetic_error, and out_of_memory
            // if ( __cmd29_expr_op30__ < 0 && __cmd29_expr_op30__ > -5 ) { return TCL_ERROR; }
            Tcl_DStringAppend(&operator_ds, "\nif ( ", -1);
            Tcl_DStringAppend(&operator_ds, varname, -1);
            Tcl_DStringAppend(&operator_ds, " < (Tcl_Obj *) 0 ", -1);
            Tcl_DStringAppend(&operator_ds, " && ", -1);
            Tcl_DStringAppend(&operator_ds, varname, -1);
            Tcl_DStringAppend(&operator_ds, " > (Tcl_Obj *) -5 ) {", -1);
            thtml_CGarbageCollection(interp, codearrVar_ptr, &operator_ds);
            Tcl_DStringAppend(&operator_ds, "\nSetResult(\"math error in CAppendExpr_Operator\");  return TCL_ERROR; }", -1);

            Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&operator_ds), Tcl_DStringLength(&operator_ds));
            Tcl_DStringFree(&operator_ds);

            Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(", -1);
            Tcl_DStringAppend(ds_ptr, varname, -1);
            Tcl_DStringAppend(ds_ptr, ");", -1);

            Tcl_DStringAppend(cmd_ds_ptr, varname, -1);

            Tcl_DStringAppend(after_ds_ptr, "\nTcl_DecrRefCount(", -1);
            Tcl_DStringAppend(after_ds_ptr, varname, -1);
            Tcl_DStringAppend(after_ds_ptr, ");", -1);

        } else if (ch == '?') {
            // three operands
            op_count++;
            char op_count_str[12];
            snprintf(op_count_str, 12, "%d", op_count);

            char varname[64];
            snprintf(varname, 64, "__%s_op%s__", name, op_count_str);

            Tcl_DString operator_ds;
            Tcl_DStringInit(&operator_ds);

            Tcl_DStringAppend(&operator_ds, "\nTcl_Obj *", -1);
            Tcl_DStringAppend(&operator_ds, varname, -1);
            Tcl_DStringAppend(&operator_ds, " = __thtml_ternary__(__interp__, ", -1);

            Tcl_Token *first_operand = &parse_ptr->tokenPtr[operands_offset];
            Tcl_Size second_operand_index = operands_offset + first_operand->numComponents + 1;
            Tcl_Token *second_operand = &parse_ptr->tokenPtr[second_operand_index];
            Tcl_Size third_operand_index = second_operand_index + second_operand->numComponents + 1;

            // fprintf(stderr, "first_operand of op: %c has numComponents: %d\n", ch, first_operand->numComponents);
            if (TCL_OK !=
                thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, operands_offset, name, &operator_ds,
                                        after_ds_ptr, THTML_BOOL)) {
                return TCL_ERROR;
            }
            Tcl_DStringAppend(&operator_ds, ", ", -1);
            if (TCL_OK !=
                thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, second_operand_index, name,
                                        &operator_ds, after_ds_ptr, flags)) {
                return TCL_ERROR;
            }
            Tcl_DStringAppend(&operator_ds, ", ", -1);
            if (TCL_OK != thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, third_operand_index, name,
                                                  &operator_ds, after_ds_ptr, flags)) {
                return TCL_ERROR;
            }

            Tcl_DStringAppend(&operator_ds, ");", -1);

            // check for divided_by_zero, exponent_of_zero, general_arithmetic_error, and out_of_memory
            // if ( __cmd29_expr_op30__ < 0 && __cmd29_expr_op30__ > -5 ) { return TCL_ERROR; }
            Tcl_DStringAppend(&operator_ds, "\nif ( ", -1);
            Tcl_DStringAppend(&operator_ds, varname, -1);
            Tcl_DStringAppend(&operator_ds, " < (Tcl_Obj *) 0 ", -1);
            Tcl_DStringAppend(&operator_ds, " && ", -1);
            Tcl_DStringAppend(&operator_ds, varname, -1);
            Tcl_DStringAppend(&operator_ds, " > (Tcl_Obj *) -5 ) {", -1);
            thtml_CGarbageCollection(interp, codearrVar_ptr, &operator_ds);
            Tcl_DStringAppend(&operator_ds, "\nSetResult(\"math error in CAppendExpr_Operator\");  return TCL_ERROR; }", -1);

            Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&operator_ds), Tcl_DStringLength(&operator_ds));
            Tcl_DStringFree(&operator_ds);

            Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(", -1);
            Tcl_DStringAppend(ds_ptr, varname, -1);
            Tcl_DStringAppend(ds_ptr, ");", -1);

            Tcl_DStringAppend(cmd_ds_ptr, varname, -1);

            Tcl_DStringAppend(after_ds_ptr, "\nTcl_DecrRefCount(", -1);
            Tcl_DStringAppend(after_ds_ptr, varname, -1);
            Tcl_DStringAppend(after_ds_ptr, ");", -1);


        } else {
            SetResult("error parsing expression: unsupported operator");
            return TCL_ERROR;
        }
    } else if (operator_token->size == 2) {
        char ch1 = operator_token->start[0];
        char ch2 = operator_token->start[1];
        if ((ch1 == '&' && ch2 == '&') || (ch1 == '|' && ch2 == '|') || (ch1 == '<' && ch2 == '<') ||
            (ch1 == '>' && ch2 == '>') || (ch1 == '=' && ch2 == '=') || (ch1 == '!' && ch2 == '=') ||
            (ch1 == '<' && ch2 == '=') || (ch1 == '>' && ch2 == '=')) {
            // two operands
            op_count++;
            char op_count_str[12];
            snprintf(op_count_str, 12, "%d", op_count);

            char varname[64];
            snprintf(varname, 64, "__%s_op%s__", name, op_count_str);

            Tcl_DString operator_ds;
            Tcl_DStringInit(&operator_ds);

            Tcl_Token *first_operand = &parse_ptr->tokenPtr[operands_offset];
            Tcl_Token *second_operand = &parse_ptr->tokenPtr[operands_offset + first_operand->numComponents + 1];

            Tcl_DStringAppend(&operator_ds, "\nTcl_Obj *", -1);
            Tcl_DStringAppend(&operator_ds, varname, -1);
            Tcl_DStringAppend(&operator_ds, " = ", -1);
            Tcl_DStringAppend(&operator_ds, " __thtml_", -1);
            Tcl_DStringAppend(&operator_ds, INSTR[(operator_token->start[0] << 8) + operator_token->start[1]], -1);
            Tcl_DStringAppend(&operator_ds, "__(", -1);

            if (TCL_OK !=
                thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, operands_offset, name, &operator_ds,
                                        after_ds_ptr, THTML_NUMBER)) {
                Tcl_DStringFree(&operator_ds);
                return TCL_ERROR;
            }

            Tcl_DStringAppend(&operator_ds, ", ", -1);

            if (TCL_OK != thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr,
                                                  operands_offset + first_operand->numComponents + 1, name,
                                                  &operator_ds, after_ds_ptr, THTML_NUMBER)) {
                Tcl_DStringFree(&operator_ds);
                return TCL_ERROR;
            }

            Tcl_DStringAppend(&operator_ds, ");", -1);
            Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&operator_ds), Tcl_DStringLength(&operator_ds));
            Tcl_DStringFree(&operator_ds);

            Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(", -1);
            Tcl_DStringAppend(ds_ptr, varname, -1);
            Tcl_DStringAppend(ds_ptr, ");", -1);

            Tcl_DStringAppend(cmd_ds_ptr, varname, -1);

            Tcl_DStringAppend(after_ds_ptr, "\nTcl_DecrRefCount(", -1);
            Tcl_DStringAppend(after_ds_ptr, varname, -1);
            Tcl_DStringAppend(after_ds_ptr, ");", -1);
        } else if ((ch1 == '*' && ch2 == '*')) {
            // expon, two operands
            op_count++;
            char op_count_str[12];
            snprintf(op_count_str, 12, "%d", op_count);

            char varname[64];
            snprintf(varname, 64, "__%s_op%s__", name, op_count_str);

            Tcl_DString operator_ds;
            Tcl_DStringInit(&operator_ds);

            Tcl_DStringAppend(&operator_ds, "\nTcl_Obj *", -1);
            Tcl_DStringAppend(&operator_ds, varname, -1);
            Tcl_DStringAppend(&operator_ds, " = ", -1);
            Tcl_DStringAppend(&operator_ds, " __thtml_expon__(__interp__, ", -1);

            Tcl_Token *first_operand = &parse_ptr->tokenPtr[operands_offset];

            // fprintf(stderr, "first_operand of op: %c has numComponents: %d\n", ch, first_operand->numComponents);
            if (TCL_OK != thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, operands_offset, name,
                                                  &operator_ds, after_ds_ptr, THTML_NUMBER)) {
                Tcl_DStringFree(&operator_ds);
                return TCL_ERROR;
            }

            Tcl_DStringAppend(&operator_ds, ", ", -1);

            if (TCL_OK != thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr,
                                                  operands_offset + first_operand->numComponents + 1, name,
                                                  &operator_ds, after_ds_ptr, THTML_NUMBER)) {
                Tcl_DStringFree(&operator_ds);
                return TCL_ERROR;
            }

            Tcl_DStringAppend(&operator_ds, ");", -1);

            // check for divided_by_zero, exponent_of_zero, general_arithmetic_error, and out_of_memory
            // if ( __cmd29_expr_op30__ < 0 && __cmd29_expr_op30__ > -5 ) { return TCL_ERROR; }
            Tcl_DStringAppend(&operator_ds, "\nif ( ", -1);
            Tcl_DStringAppend(&operator_ds, varname, -1);
            Tcl_DStringAppend(&operator_ds, " < (Tcl_Obj *) 0 ", -1);
            Tcl_DStringAppend(&operator_ds, " && ", -1);
            Tcl_DStringAppend(&operator_ds, varname, -1);
            Tcl_DStringAppend(&operator_ds, " > (Tcl_Obj *) -5 ) {", -1);
            thtml_CGarbageCollection(interp, codearrVar_ptr, &operator_ds);
            Tcl_DStringAppend(&operator_ds, "\nSetResult(\"math error in CAppendExpr_Operator\"); return TCL_ERROR; }", -1);

            Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&operator_ds), Tcl_DStringLength(&operator_ds));
            Tcl_DStringFree(&operator_ds);

            Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(", -1);
            Tcl_DStringAppend(ds_ptr, varname, -1);
            Tcl_DStringAppend(ds_ptr, ");", -1);

            Tcl_DStringAppend(cmd_ds_ptr, varname, -1);

            Tcl_DStringAppend(after_ds_ptr, "\nTcl_DecrRefCount(", -1);
            Tcl_DStringAppend(after_ds_ptr, varname, -1);
            Tcl_DStringAppend(after_ds_ptr, ");", -1);
        } else if ((ch1 == 'e' && ch2 == 'q') || (ch1 == 'n' && ch2 == 'e') ||
                   (ch1 == 'i' && ch2 == 'n') || (ch1 == 'n' && ch2 == 'i')) {
            // two operands
            op_count++;
            char op_count_str[12];
            snprintf(op_count_str, 12, "%d", op_count);

            char varname[64];
            snprintf(varname, 64, "__%s_op%s__", name, op_count_str);

            Tcl_DString operator_ds;
            Tcl_DStringInit(&operator_ds);

            Tcl_Token *first_operand = &parse_ptr->tokenPtr[operands_offset];
            Tcl_Token *second_operand = &parse_ptr->tokenPtr[operands_offset + first_operand->numComponents + 1];

            Tcl_DStringAppend(&operator_ds, "\nTcl_Obj *", -1);
            Tcl_DStringAppend(&operator_ds, varname, -1);
            Tcl_DStringAppend(&operator_ds, " = ", -1);
            Tcl_DStringAppend(&operator_ds, " __thtml_", -1);
            Tcl_DStringAppend(&operator_ds, INSTR[(operator_token->start[0] << 8) + operator_token->start[1]], -1);
            Tcl_DStringAppend(&operator_ds, "__(", -1);

            if (TCL_OK !=
                thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, operands_offset, name, &operator_ds,
                                        after_ds_ptr, 0)) {
                Tcl_DStringFree(&operator_ds);
                return TCL_ERROR;
            }

            Tcl_DStringAppend(&operator_ds, ", ", -1);

            if (TCL_OK != thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr,
                                                  operands_offset + first_operand->numComponents + 1, name,
                                                  &operator_ds, after_ds_ptr, 0)) {
                Tcl_DStringFree(&operator_ds);
                return TCL_ERROR;
            }

            Tcl_DStringAppend(&operator_ds, ");", -1);
            Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&operator_ds), Tcl_DStringLength(&operator_ds));
            Tcl_DStringFree(&operator_ds);

            Tcl_DStringAppend(cmd_ds_ptr, varname, -1);

        } else {
            SetResult("error parsing expression: unsupported operator");
            return TCL_ERROR;
        }
    } else {
        SetResult("error parsing expression: unsupported operator");
        return TCL_ERROR;
    }

    return TCL_OK;
}

static int
thtml_CAppendCommand_Token(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                           Tcl_Size i, Tcl_Size *out_i, const char *name, Tcl_DString *cmd_ds_ptr, int flags) {
    if (i >= parse_ptr->numTokens) {
        return TCL_OK;
    }
    Tcl_Token *token = &parse_ptr->tokenPtr[i];
//     fprintf(stderr, "thtml_TclAppendCommand_Token: i: %d type: %d - token: %.*s - numComponents: %d\n", i, token->type,
//            token->size, token->start, token->numComponents);

    Tcl_DStringAppend(ds_ptr, "\n// AppendCommand_Token: ", -1);
    Tcl_DStringAppend(ds_ptr, token->start, token->size);

    if (token->type == TCL_TOKEN_VARIABLE) {
        // Tcl_DStringStartSublist(__ds_val1__);
//        Tcl_DStringAppend(ds_ptr, "\nTcl_DStringStartSublist(__ds_", -1);
//        Tcl_DStringAppend(ds_ptr, name, -1);
//        Tcl_DStringAppend(ds_ptr, "__);", -1);

        if (TCL_OK != thtml_CAppendVariable(interp, codearrVar_ptr, ds_ptr, parse_ptr, i, name, cmd_ds_ptr, flags)) {
            return TCL_ERROR;
        }

        // Tcl_DStringEndSublist(__ds_val1__);
//        Tcl_DStringAppend(ds_ptr, "\nTcl_DStringEndSublist(__ds_", -1);
//        Tcl_DStringAppend(ds_ptr, name, -1);
//        Tcl_DStringAppend(ds_ptr, "__);", -1);

        *out_i = i + 2;
        return TCL_OK;
    } else if (token->type == TCL_TOKEN_SIMPLE_WORD) {
        Tcl_Token *text_token = &parse_ptr->tokenPtr[i + 1];
        assert(text_token->type == TCL_TOKEN_TEXT);
        Tcl_DStringAppend(ds_ptr, "\nTcl_DStringAppendElement(__ds_", -1);
        Tcl_DStringAppend(ds_ptr, name, -1);
        Tcl_DStringAppend(ds_ptr, "__, \"", -1);
        Tcl_DStringAppend(ds_ptr, text_token->start, text_token->size);
        Tcl_DStringAppend(ds_ptr, "\");", -1);
        *out_i = i + 2;
        return TCL_OK;
    } else if (token->type == TCL_TOKEN_TEXT) {
        Tcl_DStringAppend(ds_ptr, "\nTcl_DStringAppend(__ds_", -1);
        Tcl_DStringAppend(ds_ptr, name, -1);
        Tcl_DStringAppend(ds_ptr, "__, \"", -1);
        Tcl_DStringAppend(ds_ptr, token->start, token->size);
        Tcl_DStringAppend(ds_ptr, "\", -1);", -1);
        *out_i = i + 1;
        return TCL_OK;
    } else if (token->type == TCL_TOKEN_COMMAND) {

        subcmd_count++;

        char subcmd_name[64];
        snprintf(subcmd_name, 64, "%s_subcmd%d", name, subcmd_count);

        Tcl_DStringAppend(ds_ptr, "\n// SubCommand: ", -1);
        Tcl_DStringAppend(ds_ptr, token->start, token->size);

        // Tcl_DString __ds_subcmd1_base__;
        Tcl_DStringAppend(ds_ptr, "\nTcl_DString __ds_", -1);
        Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
        Tcl_DStringAppend(ds_ptr, "_base__;", -1);

        // Tcl_DString * __ds_subcmd1__ = &__ds__subcmd1_base__;
        Tcl_DStringAppend(ds_ptr, "\nTcl_DString *__ds_", -1);
        Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
        Tcl_DStringAppend(ds_ptr, "__ = &__ds_", -1);
        Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
        Tcl_DStringAppend(ds_ptr, "_base__;", -1);

        // Tcl_DStringInit(__ds_subcmd1__);
        Tcl_DStringAppend(ds_ptr, "\nTcl_DStringInit(__ds_", -1);
        Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
        Tcl_DStringAppend(ds_ptr, "__);", -1);

        // Tcl_DStringAppend(__ds_subcmd1__, "::thtml::runtime::tcl::evaluate_script", -1);
        Tcl_DStringAppend(ds_ptr, "\nTcl_DStringAppend(__ds_", -1);
        Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
        Tcl_DStringAppend(ds_ptr, "__, \"::thtml::runtime::tcl::evaluate_script\", -1);", -1);

        // Tcl_DStringStartSublist(__ds_subcmd1__);
        Tcl_DStringAppend(ds_ptr, "\nTcl_DStringStartSublist(__ds_", -1);
        Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
        Tcl_DStringAppend(ds_ptr, "__);", -1);

        Tcl_Parse subcmd_parse;
        if (TCL_OK != Tcl_ParseCommand(interp, token->start + 1, token->size - 2, 0, &subcmd_parse)) {
            return TCL_ERROR;
        }
        int compiled_cmd;
        if (TCL_OK !=
            thtml_CCompileCommand(interp, codearrVar_ptr, ds_ptr, &subcmd_parse, subcmd_name, 1, &compiled_cmd)) {
            return TCL_ERROR;
        }

        // Tcl_DStringEndSublist(__ds__subcmd1__);
        Tcl_DStringAppend(ds_ptr, "\nTcl_DStringEndSublist(__ds_", -1);
        Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
        Tcl_DStringAppend(ds_ptr, "__);", -1);

        if (!compiled_cmd) {
            // Tcl_Obj *__val3_cmd1__ = Tcl_DStringToObj(__ds_subcmd1__);
            Tcl_DStringAppend(ds_ptr, "\nTcl_Obj *__", -1);
            Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "__ = Tcl_DStringToObj(__ds_", -1);
            Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "__);", -1);

            // Tcl_IncrRefCount(__val3_cmd1__);
            Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(__", -1);
            Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "__);", -1);

            // fprintf(stderr, "script: %s\n", Tcl_GetString(__val3_cmd1__));
//            Tcl_DStringAppend(ds_ptr, "\nfprintf(stderr, \"script: %s\\n\", Tcl_GetString(__", -1);
//            Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
//            Tcl_DStringAppend(ds_ptr, "__));", -1);


            // if (TCL_OK != Tcl_EvalObjEx(__interp__, __val3_cmd1__, TCL_EVAL_DIRECT)) { return TCL_ERROR; }
            Tcl_DStringAppend(ds_ptr, "\nif (TCL_OK != Tcl_EvalObjEx(__interp__, __", -1);
            Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "__, TCL_EVAL_DIRECT)) {", -1);
            thtml_CGarbageCollection(interp, codearrVar_ptr, ds_ptr);
            Tcl_DStringAppend(ds_ptr, "\nSetResult(\"eval command failed in CAppendCommand_Token\"); return TCL_ERROR; }", -1);

            // Tcl_DecrRefCount(__val3_cmd1__);
            Tcl_DStringAppend(ds_ptr, "\nTcl_DecrRefCount(__", -1);
            Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "__);", -1);
        }

        // Tcl_Obj *__val3_subcmd1__ = Tcl_GetObjResult(__interp__);
        Tcl_DStringAppend(ds_ptr, "\nTcl_Obj *__", -1);
        Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
        Tcl_DStringAppend(ds_ptr, "_res__ = Tcl_GetObjResult(__interp__);", -1);

        // Tcl_IncrRefCount(__val3_subcmd1_res__);
        Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(__", -1);
        Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
        Tcl_DStringAppend(ds_ptr, "_res__);", -1);

        // Tcl_ResetResult(__interp__);
        Tcl_DStringAppend(ds_ptr, "\nTcl_ResetResult(__interp__);", -1);

        if (cmd_ds_ptr != NULL) {
            Tcl_DStringAppend(cmd_ds_ptr, "__", -1);
            Tcl_DStringAppend(cmd_ds_ptr, subcmd_name, -1);
            Tcl_DStringAppend(cmd_ds_ptr, "_res__", -1);
        } else {

            // Tcl_DStringAppend(__ds_val3__, Tcl_GetString(__val3_subcmd1__), -1);
            Tcl_DStringAppend(ds_ptr, "\nTcl_DStringAppend(__ds_", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "__, Tcl_GetString(__", -1);
            Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "_res__), -1);", -1);
        }

        // Tcl_DecrRefCount(__val3_subcmd1_res__);
        Tcl_DStringAppend(ds_ptr, "\nTcl_DecrRefCount(__", -1);
        Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
        Tcl_DStringAppend(ds_ptr, "_res__);", -1);

        // Tcl_DStringFree(__ds_subcmd1__);
        Tcl_DStringAppend(ds_ptr, "\nTcl_DStringFree(__ds_", -1);
        Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
        Tcl_DStringAppend(ds_ptr, "__);", -1);

    } else if (token->type == TCL_TOKEN_EXPAND_WORD) {
        SetResult("error parsing expression: expand word not supported");
        return TCL_ERROR;
    } else if (token->type == TCL_TOKEN_WORD) {

        static int word_token_count = 0;
        word_token_count++;

        char word_token_name[64];
        snprintf(word_token_name, 64, "wt%d", word_token_count);

        Tcl_DStringAppend(ds_ptr, "\n// TOKEN_WORD: ", -1);
        Tcl_DStringAppend(ds_ptr, token->start, token->size);
        Tcl_DStringAppend(ds_ptr, " numComponents: ", -1);
        Tcl_DStringAppend(ds_ptr, Tcl_GetString(Tcl_NewIntObj(token->numComponents)), -1);

        // Tcl_DString __ds__wt1_base__;
        Tcl_DStringAppend(ds_ptr, "\nTcl_DString __ds_", -1);
        Tcl_DStringAppend(ds_ptr, word_token_name, -1);
        Tcl_DStringAppend(ds_ptr, "_base__;", -1);

        // Tcl_DString * __ds__wt1__ = &__ds__wt1_base__;
        Tcl_DStringAppend(ds_ptr, "\nTcl_DString *__ds_", -1);
        Tcl_DStringAppend(ds_ptr, word_token_name, -1);
        Tcl_DStringAppend(ds_ptr, "__ = &__ds_", -1);
        Tcl_DStringAppend(ds_ptr, word_token_name, -1);
        Tcl_DStringAppend(ds_ptr, "_base__;", -1);

        // Tcl_DStringInit(__ds__wt1__);
        Tcl_DStringAppend(ds_ptr, "\nTcl_DStringInit(__ds_", -1);
        Tcl_DStringAppend(ds_ptr, word_token_name, -1);
        Tcl_DStringAppend(ds_ptr, "__);", -1);

        Tcl_Size start_i = i;
        Tcl_Size j = i + 1;
        while (i - start_i - 1 < token->numComponents) {

            if (TCL_OK != thtml_CAppendCommand_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, j, &i, word_token_name,
                                                     cmd_ds_ptr, flags)) {
                return TCL_ERROR;
            }

            j = i;
        }

        // Tcl_DStringAppendElement(__ds_val1__, Tcl_DStringValue(__ds_wt1__)));
        Tcl_DStringAppend(ds_ptr, "\nTcl_DStringAppendElement(__ds_", -1);
        Tcl_DStringAppend(ds_ptr, name, -1);
        Tcl_DStringAppend(ds_ptr, "__, Tcl_DStringValue(__ds_", -1);
        Tcl_DStringAppend(ds_ptr, word_token_name, -1);
        Tcl_DStringAppend(ds_ptr, "__));", -1);

        // Tcl_DStringFree(__ds_wt1__);
        Tcl_DStringAppend(ds_ptr, "\nTcl_DStringFree(__ds_", -1);
        Tcl_DStringAppend(ds_ptr, word_token_name, -1);
        Tcl_DStringAppend(ds_ptr, "__);", -1);


        *out_i = i + 1;
        return TCL_OK;
    } else if (token->type == TCL_TOKEN_BS) {
        Tcl_DStringAppend(ds_ptr, token->start, token->size);
    } else {
        fprintf(stderr, "TclAppendCommand_Token: other type of token: %d\n", token->type);
    }
    *out_i = i + 1;
    return TCL_OK;
}

static int
thtml_CAppendExpr_Token(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                        Tcl_Size i, const char *name, Tcl_DString *expr_ds_ptr, Tcl_DString *after_ds_ptr, int flags) {
    Tcl_Token *token = &parse_ptr->tokenPtr[i];

    if (token->type == TCL_TOKEN_SUB_EXPR) {
        Tcl_Token *next_token = &parse_ptr->tokenPtr[i + 1];
        Tcl_DStringAppend(expr_ds_ptr, "(", 1);
        if (next_token->type == TCL_TOKEN_OPERATOR) {
            // If the first sub-token after the TCL_TOKEN_SUB_EXPR token is a TCL_TOKEN_OPERATOR token,
            // the subexpression consists of an operator and its token operands.
            if (TCL_OK != thtml_CAppendExpr_Operator(interp, codearrVar_ptr, ds_ptr, parse_ptr, i, name, expr_ds_ptr,
                                                     after_ds_ptr, flags)) {
                return TCL_ERROR;
            }
        } else if (next_token->type == TCL_TOKEN_TEXT) {
            if (TCL_OK != thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, i + 1, name, expr_ds_ptr,
                                                  after_ds_ptr, flags)) {
                return TCL_ERROR;
            }
        } else {
            // Otherwise, the subexpression is a value described by one of the token types TCL_TOKEN_WORD,
            // TCL_TOKEN_BS, TCL_TOKEN_COMMAND, TCL_TOKEN_VARIABLE, and TCL_TOKEN_SUB_EXPR.
            if (TCL_OK != thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, i + 1, name, expr_ds_ptr,
                                                  after_ds_ptr, flags)) {
                return TCL_ERROR;
            }
        }
        Tcl_DStringAppend(expr_ds_ptr, ")", 1);
    } else if (token->type == TCL_TOKEN_VARIABLE) {
        return thtml_CAppendVariable(interp, codearrVar_ptr, ds_ptr, parse_ptr, i, "default", expr_ds_ptr, flags);
    } else if (token->type == TCL_TOKEN_TEXT) {
        count_text_subst++;
        char count_text_subst_str[12];
        snprintf(count_text_subst_str, 12, "%d", count_text_subst);

        char varname[64];
        snprintf(varname, 64, "__%s_text%s__", name, count_text_subst_str);

        Tcl_DStringAppend(ds_ptr, "\nTcl_Obj *", -1);
        Tcl_DStringAppend(ds_ptr, varname, -1);
        Tcl_DStringAppend(ds_ptr, " = Tcl_NewStringObj(\"", -1);
        Tcl_DStringAppend(ds_ptr, token->start, token->size);
        Tcl_DStringAppend(ds_ptr, "\", -1);", -1);

        // incr ref count
        Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(", -1);
        Tcl_DStringAppend(ds_ptr, varname, -1);
        Tcl_DStringAppend(ds_ptr, ");", -1);

        if (flags & THTML_NUMBER) {
            thtml_CAppendCheck_Number(interp, codearrVar_ptr, ds_ptr, varname);
        } else if (flags & THTML_BOOL) {
            thtml_CAppendCheck_Bool(interp, codearrVar_ptr, ds_ptr, varname);
        }

        // add to expression
        Tcl_DStringAppend(expr_ds_ptr, varname, -1);

        // decr ref count
        Tcl_DStringAppend(after_ds_ptr, "\nTcl_DecrRefCount(", -1);
        Tcl_DStringAppend(after_ds_ptr, varname, -1);
        Tcl_DStringAppend(after_ds_ptr, ");", -1);

    } else if (token->type == TCL_TOKEN_COMMAND) {
        if (TCL_OK !=
            thtml_CAppendCommand_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, i, &i, name, expr_ds_ptr, flags)) {
            return TCL_ERROR;
        }
    } else if (token->type == TCL_TOKEN_EXPAND_WORD) {
        SetResult("error parsing expression: expand word not supported");
        return TCL_ERROR;
    } else if (token->type == TCL_TOKEN_WORD) {
//        Tcl_DStringAppend(expr_ds_ptr, "\"", 1);
        for (int j = 0; j < token->numComponents; j++) {
            if (TCL_OK !=
                thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, i + 1 + j, name, expr_ds_ptr,
                                        after_ds_ptr, flags)) {
                return TCL_ERROR;
            }
        }
//        Tcl_DStringAppend(expr_ds_ptr, "\"", 1);
    } else if (token->type == TCL_TOKEN_BS) {
        Tcl_DStringAppend(expr_ds_ptr, token->start, token->size);
    } else {
        fprintf(stderr, "other type of token: %d\n", token->type);
    }
    return TCL_OK;
}

int thtml_CCompileExpr(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                       const char *name) {
    // After Tcl_ParseExpr returns, the first token pointed to by the tokenPtr field of the Tcl_Parse structure
    // always has type TCL_TOKEN_SUB_EXPR. It is followed by the sub-tokens that must be evaluated to produce
    // the value of the expression. Only the token information in the Tcl_Parse structure is modified:
    // the commentStart, commentSize, commandStart, and commandSize fields are not modified by Tcl_ParseExpr.

    Tcl_DString expr_ds;
    Tcl_DStringInit(&expr_ds);
    Tcl_DStringAppend(&expr_ds, "\nTcl_Obj *__", -1);
    Tcl_DStringAppend(&expr_ds, name, -1);
    Tcl_DStringAppend(&expr_ds, "__ = ", -1);

    Tcl_DString after_ds;
    Tcl_DStringInit(&after_ds);

    if (TCL_OK !=
        thtml_CAppendExpr_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, 0, name, &expr_ds, &after_ds, 0)) {
        Tcl_DStringFree(&expr_ds);
        Tcl_DStringFree(&after_ds);
        return TCL_ERROR;
    }

    Tcl_DStringAppend(&expr_ds, ";", -1);
    Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&expr_ds), Tcl_DStringLength(&expr_ds));
    Tcl_DStringFree(&expr_ds);

    Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(__", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__);", -1);

    Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&after_ds), Tcl_DStringLength(&after_ds));
    Tcl_DStringFree(&after_ds);

    return TCL_OK;
}

int
thtml_CCompileQuotedString(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                           const char *name) {
    for (int i = 0; i < parse_ptr->numTokens; i++) {
        Tcl_Token *token = &parse_ptr->tokenPtr[i];
        if (token->type == TCL_TOKEN_TEXT) {
            Tcl_DStringAppend(ds_ptr, token->start, token->size);
        } else if (token->type == TCL_TOKEN_BS) {
            Tcl_DStringAppend(ds_ptr, token->start, token->size);
        } else if (token->type == TCL_TOKEN_COMMAND) {
            SetResult("error parsing quoted string: command substitution not supported");
            return TCL_ERROR;
        } else if (token->type == TCL_TOKEN_VARIABLE) {
            if (TCL_OK != thtml_CAppendVariable(interp, codearrVar_ptr, ds_ptr, parse_ptr, i, name, NULL, 0)) {
                return TCL_ERROR;
            }
            i++;
        } else {
            fprintf(stderr, "type: %d\n", token->type);
            SetResult("error parsing quoted string: unsupported token type");
            return TCL_ERROR;
        }
    }
    return TCL_OK;
}

int
thtml_CCompileTemplateText(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr) {
    for (int i = 0; i < parse_ptr->numTokens; i++) {
        Tcl_Token *token = &parse_ptr->tokenPtr[i];
        if (token->type == TCL_TOKEN_TEXT) {
            Tcl_DStringAppend(ds_ptr, token->start, token->size);
        } else if (token->type == TCL_TOKEN_BS) {
//            fprintf(stderr, "TCL_TOKEN_BS: %.*s\n", token->size, token->start);
            Tcl_DStringAppend(ds_ptr, token->start, token->size);
        } else if (token->type == TCL_TOKEN_COMMAND) {

            template_cmd_count++;

            char cmd_name[64];
            snprintf(cmd_name, 64, "cmd%d", template_cmd_count);

            Tcl_Parse cmd_parse;
            if (TCL_OK != Tcl_ParseCommand(interp, token->start + 1, token->size - 2, 0, &cmd_parse)) {
                Tcl_FreeParse(&cmd_parse);
                return TCL_ERROR;
            }

            Tcl_DString cmd_ds;
            Tcl_DStringInit(&cmd_ds);

            int compiled_cmd = 0;
            if (TCL_OK !=
                thtml_CCompileCommand(interp, codearrVar_ptr, &cmd_ds, &cmd_parse, cmd_name, 0, &compiled_cmd)) {
                Tcl_FreeParse(&cmd_parse);
                Tcl_DStringFree(&cmd_ds);
                return TCL_ERROR;
            }

            Tcl_DStringAppend(ds_ptr, "\x03", -1);

            if (!compiled_cmd) {
                // Tcl_DString __ds_cmd1_base__;
                Tcl_DStringAppend(ds_ptr, "\nTcl_DString __ds_", -1);
                Tcl_DStringAppend(ds_ptr, cmd_name, -1);
                Tcl_DStringAppend(ds_ptr, "_base__", -1);

                // Tcl_DString *__ds_cmd1__ = &__ds_cmd1_base__;
                Tcl_DStringAppend(ds_ptr, ";\nTcl_DString *__ds_", -1);
                Tcl_DStringAppend(ds_ptr, cmd_name, -1);
                Tcl_DStringAppend(ds_ptr, "__ = &__ds_", -1);
                Tcl_DStringAppend(ds_ptr, cmd_name, -1);
                Tcl_DStringAppend(ds_ptr, "_base__;", -1);

                // Tcl_DStringInit(__ds_cmd1__);
                Tcl_DStringAppend(ds_ptr, "\nTcl_DStringInit(__ds_", -1);
                Tcl_DStringAppend(ds_ptr, cmd_name, -1);
                Tcl_DStringAppend(ds_ptr, "__);", -1);
            }

            Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&cmd_ds), Tcl_DStringLength(&cmd_ds));
            Tcl_DStringFree(&cmd_ds);

            if (!compiled_cmd) {
                // if (TCL_OK != Tcl_EvalObjEx(interp, Tcl_DStringToObj(__ds_cmd1__), TCL_EVAL_DIRECT)) { return TCL_ERROR; }
                Tcl_DStringAppend(ds_ptr,
                                  "\nif (TCL_OK != Tcl_EvalObjEx(__interp__, Tcl_DStringToObj(__ds_",
                                  -1);
                Tcl_DStringAppend(ds_ptr, cmd_name, -1);
                Tcl_DStringAppend(ds_ptr, "__), TCL_EVAL_DIRECT)) {", -1);
                thtml_CGarbageCollection(interp, codearrVar_ptr, ds_ptr);
                Tcl_DStringAppend(ds_ptr, "\nSetResult(\"eval command failed in CCompileTemplateText\"); return TCL_ERROR; }", -1);
            }

            // Tcl_Obj *__cmd1__ = Tcl_GetObjResult(__interp__);
            Tcl_DStringAppend(ds_ptr, "\nTcl_Obj *__", -1);
            Tcl_DStringAppend(ds_ptr, cmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "_res__ = Tcl_GetObjResult(__interp__);", -1);

            // Tcl_IncrRefCount(__cmd1__);
            Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(__", -1);
            Tcl_DStringAppend(ds_ptr, cmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "_res__);", -1);

            // Tcl_ResetResult(__interp__);
            Tcl_DStringAppend(ds_ptr, "\nTcl_ResetResult(__interp__);", -1);

            // Tcl_DStringAppend(__ds_default__, Tcl_GetString(__cmd1__), -1);
            Tcl_DStringAppend(ds_ptr, "\nTcl_DStringAppend(__ds_default__, Tcl_GetString(__", -1);
            Tcl_DStringAppend(ds_ptr, cmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "_res__), -1);", -1);

            // Tcl_DecrRefCount(__cmd1__);
            Tcl_DStringAppend(ds_ptr, "\nTcl_DecrRefCount(__", -1);
            Tcl_DStringAppend(ds_ptr, cmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "_res__);", -1);

            if (!compiled_cmd) {
                // Tcl_DStringFree(__ds_cmd1__);
                Tcl_DStringAppend(ds_ptr, "\nTcl_DStringFree(__ds_", -1);
                Tcl_DStringAppend(ds_ptr, cmd_name, -1);
                Tcl_DStringAppend(ds_ptr, "__);", -1);
            }

            Tcl_DStringAppend(ds_ptr, "\x02", -1);

            Tcl_FreeParse(&cmd_parse);
        } else if (token->type == TCL_TOKEN_VARIABLE) {
            Tcl_DStringAppend(ds_ptr, "\x03", -1);
            if (TCL_OK != thtml_CAppendVariable(interp, codearrVar_ptr, ds_ptr, parse_ptr, i, "default", NULL, 0)) {
                return TCL_ERROR;
            }
            Tcl_DStringAppend(ds_ptr, "\n\x02", -1);
            i++;
        } else {
            SetResult("error parsing quoted string: unsupported token type");
            return TCL_ERROR;
        }
    }
    return TCL_OK;
}

int thtml_CCompileExprCommand(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                              const char *name) {

    Tcl_Token *token = &parse_ptr->tokenPtr[2];

    Tcl_Parse expr_parse;
    if (TCL_OK != Tcl_ParseExpr(interp, &token->start[1], token->size - 2, &expr_parse)) {
        Tcl_FreeParse(&expr_parse);
        return TCL_ERROR;
    }

    char expr_name[64];
    snprintf(expr_name, 64, "%s_expr", name);

    if (TCL_OK != thtml_CCompileExpr(interp, codearrVar_ptr, ds_ptr, &expr_parse, expr_name)) {
        return TCL_ERROR;
    }

    Tcl_FreeParse(&expr_parse);

    // Tcl_SetObjResult(__interp__, Tcl_NewIntObj(__val3_cmd1__));
    Tcl_DStringAppend(ds_ptr, "\nTcl_SetObjResult(__interp__, __", -1);
    Tcl_DStringAppend(ds_ptr, expr_name, -1);
    Tcl_DStringAppend(ds_ptr, "__);", -1);

    // Tcl_DecrRefCount(__val3_cmd1__);
    Tcl_DStringAppend(ds_ptr, "\nTcl_DecrRefCount(__", -1);
    Tcl_DStringAppend(ds_ptr, expr_name, -1);
    Tcl_DStringAppend(ds_ptr, "__);", -1);

    return TCL_OK;
}

int
thtml_CCompileGenericCommand(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                             const char *name) {
    // print all tokens
//    for (int i = 0; i < parse_ptr->numTokens; i++) {
//        Tcl_Token *token = &parse_ptr->tokenPtr[i];
//        fprintf(stderr, "lindex: token %d: type: %d - token: %.*s - numComponents: %d\n", i, token->type,
//                token->size, token->start, token->numComponents);
//    }

    if (parse_ptr->tokenPtr[2].numComponents != 2) {
        SetResult("error parsing expression: lindex requires two arguments");
        return TCL_ERROR;
    }

    Tcl_Token *token = &parse_ptr->tokenPtr[0];

    Tcl_DString cmd_ds;
    Tcl_DStringInit(&cmd_ds);
    Tcl_DStringAppend(&cmd_ds, "\nif (TCL_OK != __thtml_", -1);
    Tcl_DStringAppend(&cmd_ds, token->start, token->size);
    Tcl_DStringAppend(&cmd_ds, "_", -1);
    Tcl_DStringAppend(&cmd_ds, Tcl_GetString(Tcl_NewIntObj(parse_ptr->tokenPtr[2].numComponents)), -1);
    Tcl_DStringAppend(&cmd_ds, "(__interp__, ", -1);

    int in_eval_p = 0;
    Tcl_Size i = 2;
    int first = 1;
    while (i < parse_ptr->numTokens) {
        if (!first) {
            Tcl_DStringAppend(&cmd_ds, ", ", -1);
        }
        if (TCL_OK !=
            thtml_CAppendCommand_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, i, &i, name, &cmd_ds, in_eval_p)) {
            fprintf(stderr, "thtml_CAppendCommand_Token failed\n");
            return TCL_ERROR;
        }
        first = 0;
    }
    Tcl_DStringAppend(&cmd_ds, ")) {", -1);
    thtml_CGarbageCollection(interp, codearrVar_ptr, &cmd_ds);
    Tcl_DStringAppend(&cmd_ds, "\nSetResult(\"failure in CCompileGenericCommand\"); return TCL_ERROR; }", -1);

    Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&cmd_ds), Tcl_DStringLength(&cmd_ds));
    Tcl_DStringFree(&cmd_ds);

    return TCL_OK;
}

int thtml_CCompileCommand(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                          const char *name, int nested, int *compiled_command) {
    DBG(fprintf(stderr, "thtml_CCompileCommand\n"));

    Tcl_Token *token = &parse_ptr->tokenPtr[0];
    // if it is "expr" command, then call thtml_CCompileExpr
    if (token->type == TCL_TOKEN_SIMPLE_WORD) {
        if (token->size == 4 && 0 == strncmp(token->start, "expr", 4) && parse_ptr->numTokens == 4) {
            if (TCL_OK != thtml_CCompileExprCommand(interp, codearrVar_ptr, ds_ptr, parse_ptr, name)) {
                return TCL_ERROR;
            }
            *compiled_command = 1;
            return TCL_OK;
//        } else if (token->size == 6 && 0 == strncmp(token->start, "lindex", 6)) {
//            if (TCL_OK != thtml_CCompileGenericCommand(interp, codearrVar_ptr, ds_ptr, parse_ptr, name)) {
//                return TCL_ERROR;
//            }
//            *compiled_command = 1;
//            return TCL_OK;
        }
    }

    int in_eval_p = 1;  // it is not a compiled command, so we are in eval
    Tcl_Size i = 0;
    int first = 1;
    while (i < parse_ptr->numTokens) {
        if (!first) {
            Tcl_DStringAppend(ds_ptr, "\nTcl_DStringAppend(__ds_", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "__, \" \", -1);", -1);
        }

        if (TCL_OK !=
            thtml_CAppendCommand_Token(interp, codearrVar_ptr, ds_ptr, parse_ptr, i, &i, name, NULL, in_eval_p)) {
            fprintf(stderr, "thtml_CAppendCommand_Token failed\n");
            return TCL_ERROR;
        }

        first = 0;
    }

    *compiled_command = 0;
    return TCL_OK;
}

int
thtml_CCompileForeachList(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                          const char *name) {
    Tcl_DStringAppend(ds_ptr, "Tcl_DString __ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "_base__;", -1);
    Tcl_DStringAppend(ds_ptr, "\nTcl_DString *__ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__ = &__ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "_base__;", -1);
    Tcl_DStringAppend(ds_ptr, "\nTcl_DStringInit(__ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__);\n", -1);

    for (int i = 0; i < parse_ptr->numTokens; i++) {
        Tcl_Token *token = &parse_ptr->tokenPtr[i];
        if (token->type == TCL_TOKEN_TEXT) {
            Tcl_DStringAppend(ds_ptr, "\nTcl_DStringAppend(__ds_", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "__, \"", -1);
            Tcl_DStringAppend(ds_ptr, token->start, token->size);
            Tcl_DStringAppend(ds_ptr, "\", -1);", -1);
        } else if (token->type == TCL_TOKEN_BS) {
            Tcl_DStringAppend(ds_ptr, token->start, token->size);
        } else if (token->type == TCL_TOKEN_COMMAND) {

            foreach_cmd_count++;

            char cmd_name[64];
            snprintf(cmd_name, 64, "forcmd%d", foreach_cmd_count);

            Tcl_Parse cmd_parse;
            if (TCL_OK != Tcl_ParseCommand(interp, token->start + 1, token->size - 2, 0, &cmd_parse)) {
                Tcl_FreeParse(&cmd_parse);
                return TCL_ERROR;
            }

            Tcl_DString cmd_ds;
            Tcl_DStringInit(&cmd_ds);

            int compiled_cmd = 0;
            if (TCL_OK !=
                thtml_CCompileCommand(interp, codearrVar_ptr, &cmd_ds, &cmd_parse, cmd_name, 0, &compiled_cmd)) {
                Tcl_FreeParse(&cmd_parse);
                Tcl_DStringFree(&cmd_ds);
                return TCL_ERROR;
            }

//            Tcl_DStringAppend(ds_ptr, "\x03", -1);

            if (!compiled_cmd) {
                // Tcl_DString __ds_cmd1_base__;
                Tcl_DStringAppend(ds_ptr, "\nTcl_DString __ds_", -1);
                Tcl_DStringAppend(ds_ptr, cmd_name, -1);
                Tcl_DStringAppend(ds_ptr, "_base__", -1);

                // Tcl_DString *__ds_cmd1__ = &__ds_cmd1_base__;
                Tcl_DStringAppend(ds_ptr, ";\nTcl_DString *__ds_", -1);
                Tcl_DStringAppend(ds_ptr, cmd_name, -1);
                Tcl_DStringAppend(ds_ptr, "__ = &__ds_", -1);
                Tcl_DStringAppend(ds_ptr, cmd_name, -1);
                Tcl_DStringAppend(ds_ptr, "_base__;", -1);

                // Tcl_DStringInit(__ds_cmd1__);
                Tcl_DStringAppend(ds_ptr, "\nTcl_DStringInit(__ds_", -1);
                Tcl_DStringAppend(ds_ptr, cmd_name, -1);
                Tcl_DStringAppend(ds_ptr, "__);", -1);
            }

            Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&cmd_ds), Tcl_DStringLength(&cmd_ds));
            Tcl_DStringFree(&cmd_ds);

            if (!compiled_cmd) {
                // if (TCL_OK != Tcl_EvalObjEx(interp, Tcl_DStringToObj(__ds_cmd1__), TCL_EVAL_DIRECT)) { return TCL_ERROR; }
                Tcl_DStringAppend(ds_ptr,
                                  "\nif (TCL_OK != Tcl_EvalObjEx(__interp__, Tcl_DStringToObj(__ds_",
                                  -1);
                Tcl_DStringAppend(ds_ptr, cmd_name, -1);
                Tcl_DStringAppend(ds_ptr, "__), TCL_EVAL_DIRECT)) {", -1);
                thtml_CGarbageCollection(interp, codearrVar_ptr, ds_ptr);
                Tcl_DStringAppend(ds_ptr, "\nSetResult(\"eval failed in CCompileForeachList\"); return TCL_ERROR; }", -1);
            }

            // Tcl_Obj *__cmd1__ = Tcl_GetObjResult(__interp__);
            Tcl_DStringAppend(ds_ptr, "\nTcl_Obj *__", -1);
            Tcl_DStringAppend(ds_ptr, cmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "_res__ = Tcl_GetObjResult(__interp__);", -1);

            // Tcl_IncrRefCount(__cmd1_res__);
            Tcl_DStringAppend(ds_ptr, "\nTcl_IncrRefCount(__", -1);
            Tcl_DStringAppend(ds_ptr, cmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "_res__);", -1);

            // Tcl_ResetResult(__interp__);
            Tcl_DStringAppend(ds_ptr, "\nTcl_ResetResult(__interp__);", -1);

            // Tcl_DStringAppendElement(__ds_list1__, Tcl_GetString(__forcmd1__));
            Tcl_DStringAppend(ds_ptr, "\nTcl_DStringAppendElement(__ds_", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "__, Tcl_GetString(__", -1);
            Tcl_DStringAppend(ds_ptr, cmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "_res__));", -1);

            // Tcl_DecrRefCount(__cmd1_res__);
            Tcl_DStringAppend(ds_ptr, "\nTcl_DecrRefCount(__", -1);
            Tcl_DStringAppend(ds_ptr, cmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "_res__);", -1);

            if (!compiled_cmd) {
                // Tcl_DStringFree(__ds_forcmd1__);
                Tcl_DStringAppend(ds_ptr, "\nTcl_DStringFree(__ds_", -1);
                Tcl_DStringAppend(ds_ptr, cmd_name, -1);
                Tcl_DStringAppend(ds_ptr, "__);", -1);
            }

//            Tcl_DStringAppend(ds_ptr, "\x02", -1);

            Tcl_FreeParse(&cmd_parse);

        } else if (token->type == TCL_TOKEN_VARIABLE) {
            if (TCL_OK != thtml_CAppendVariable(interp, codearrVar_ptr, ds_ptr, parse_ptr, i, name, NULL, 0)) {
                return TCL_ERROR;
            }
            i++;
        } else {
            fprintf(stderr, "type: %d\n", token->type);
            SetResult("error parsing quoted string: unsupported token type");
            return TCL_ERROR;
        }
    }

    // Tcl_Obj *__list1__ = Tcl_DStringToObj(__ds_list1__);
    Tcl_DStringAppend(ds_ptr, "\nTcl_Obj *__", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__ = Tcl_DStringToObj(__ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__);", -1);

    // Tcl_DStringFree(__ds_list1__);
    Tcl_DStringAppend(ds_ptr, "\nTcl_DStringFree(__ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__);", -1);

    return TCL_OK;
}

int thtml_CCompileForeachListCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "CCompileForeachListCmd\n"));

    CheckArgs(4, 4, 1, "codearrVar text name");

    Tcl_Size text_length;
    char *text = Tcl_GetStringFromObj(objv[2], &text_length);

    Tcl_Parse parse;
    if (TCL_OK != Tcl_ParseQuotedString(interp, text, text_length, &parse, 0, NULL)) {
        Tcl_FreeParse(&parse);
        return TCL_ERROR;
    }

    Tcl_Size name_length;
    char *name = Tcl_GetStringFromObj(objv[3], &name_length);

    Tcl_DString ds;
    Tcl_DStringInit(&ds);

    if (TCL_OK != thtml_CCompileForeachList(interp, objv[1], &ds, &parse, name)) {
        Tcl_FreeParse(&parse);
        Tcl_DStringFree(&ds);
        return TCL_ERROR;
    }

    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

int
thtml_CCompileQuotedArg(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                        const char *name) {
    Tcl_DStringAppend(ds_ptr, "\nTcl_DString __ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "_base__;", -1);
    Tcl_DStringAppend(ds_ptr, "\nTcl_DString *__ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__ = &__ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "_base__;", -1);
    Tcl_DStringAppend(ds_ptr, "\nTcl_DStringInit(__ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__);\n", -1);
    for (int i = 0; i < parse_ptr->numTokens; i++) {
        Tcl_Token *token = &parse_ptr->tokenPtr[i];
        if (token->type == TCL_TOKEN_TEXT) {
            Tcl_DStringAppend(ds_ptr, "\nTcl_DStringAppend(__ds_", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "__, \"", -1);
            Tcl_DStringAppend(ds_ptr, token->start, token->size);
            Tcl_DStringAppend(ds_ptr, "\", -1);", -1);
        } else if (token->type == TCL_TOKEN_BS) {
            Tcl_DStringAppend(ds_ptr, "\nTcl_DStringAppend(__ds_", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "__, \"", -1);
            Tcl_DStringAppend(ds_ptr, token->start, token->size);
            Tcl_DStringAppend(ds_ptr, "\", -1);", -1);
        } else if (token->type == TCL_TOKEN_COMMAND) {
            SetResult("error parsing quoted string: command substitution not supported");
            return TCL_ERROR;
        } else if (token->type == TCL_TOKEN_VARIABLE) {
            if (TCL_OK != thtml_CAppendVariable(interp, codearrVar_ptr, ds_ptr, parse_ptr, i, name, NULL, 0)) {
                return TCL_ERROR;
            }
            i++;
        } else {
            fprintf(stderr, "type: %d\n", token->type);
            SetResult("error parsing quoted string: unsupported token type");
            return TCL_ERROR;
        }
    }

    Tcl_DStringAppend(ds_ptr, "\nTcl_Obj *__", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__ = Tcl_DStringToObj(__ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__);", -1);
    Tcl_DStringAppend(ds_ptr, "\nTcl_DStringFree(__ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__);\n", -1);
    return TCL_OK;
}

int thtml_CCompileQuotedArgCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "CCompileQuotedArgCmd\n"));

    CheckArgs(4, 4, 1, "codearrVar text name");

    Tcl_Size text_length;
    char *text = Tcl_GetStringFromObj(objv[2], &text_length);

    Tcl_Size name_length;
    char *name = Tcl_GetStringFromObj(objv[3], &name_length);

    Tcl_Parse parse;
    if (TCL_OK != Tcl_ParseQuotedString(interp, text, text_length, &parse, 0, NULL)) {
        Tcl_FreeParse(&parse);
        return TCL_ERROR;
    }

    Tcl_DString ds;
    Tcl_DStringInit(&ds);

    if (TCL_OK != thtml_CCompileQuotedArg(interp, objv[1], &ds, &parse, name)) {
        Tcl_FreeParse(&parse);
        Tcl_DStringFree(&ds);
        return TCL_ERROR;
    }

    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}
