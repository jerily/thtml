/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "common.h"
#include "library.h"
#include "compiler_tcl.h"
#include "md5.h"

#include <stdio.h>
#include <string.h>

static int           thtml_ModuleInitialized;

static void thtml_AppendEscaped(const char *p, const char *end, Tcl_DString *dsPtr) {
    while (p < end) {
        if (*p == '\n') {
            Tcl_DStringAppend(dsPtr, "\\n", 2);
        } else if (*p == '\r') {
            Tcl_DStringAppend(dsPtr, "\\r", 2);
        } else if (*p == '"') {
            Tcl_DStringAppend(dsPtr, "\\\"", 2);
        } else if (*p == '\\') {
            Tcl_DStringAppend(dsPtr, "\\\\", 2);
        } else {
            Tcl_DStringAppend(dsPtr, p, 1);
        }
        p++;
    }
}

static int thtml_TclTransformCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr, "TclTransformCmd\n"));

    CheckArgs(2,2,1,"intermediate_code");

    int intermediate_code_length;
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
                Tcl_DStringAppend(&ds, "\nappend ds \"", 12);
                thtml_AppendEscaped(p, q, &ds);
                Tcl_DStringAppend(&ds, "\"\n", 2);
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

static int thtml_TclCompileExprCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr, "TclCompileExprCmd\n"));

    CheckArgs(3, 3, 1, "codearrVar text");

    int text_length;
    char *text = Tcl_GetStringFromObj(objv[2], &text_length);

    Tcl_Parse parse;
    if (TCL_OK != Tcl_ParseExpr(interp, text, text_length, &parse)) {
        Tcl_FreeParse(&parse);
        return TCL_ERROR;
    }

    Tcl_Obj *blocks_key_ptr = Tcl_NewStringObj("blocks", 6);
    Tcl_IncrRefCount(blocks_key_ptr);
    Tcl_Obj *blocks_list_ptr = Tcl_ObjGetVar2(interp, objv[1], blocks_key_ptr, TCL_LEAVE_ERR_MSG);
    Tcl_DecrRefCount(blocks_key_ptr);

    if (blocks_list_ptr == NULL) {
        Tcl_FreeParse(&parse);
        SetResult("error getting blocks from codearr");
        return TCL_ERROR;
    }

    Tcl_DString ds;
    Tcl_DStringInit(&ds);

    if (TCL_OK != thtml_TclCompileExpr(interp, blocks_list_ptr, &ds, &parse)) {
        Tcl_FreeParse(&parse);
        Tcl_DStringFree(&ds);
        return TCL_ERROR;
    }

    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

static int thtml_TclCompileQuotedStringCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr, "TclCompileQuotedStringCmd\n"));

    CheckArgs(3, 3, 1, "codearrVar text");

    int text_length;
    char *text = Tcl_GetStringFromObj(objv[2], &text_length);

    Tcl_Parse parse;
    if (TCL_OK != Tcl_ParseQuotedString(interp, text, text_length, &parse, 0, NULL)) {
        Tcl_FreeParse(&parse);
        return TCL_ERROR;
    }

    Tcl_Obj *blocks_key_ptr = Tcl_NewStringObj("blocks", 6);
    Tcl_IncrRefCount(blocks_key_ptr);
    Tcl_Obj *blocks_list_ptr = Tcl_ObjGetVar2(interp, objv[1], blocks_key_ptr, TCL_LEAVE_ERR_MSG);
    Tcl_DecrRefCount(blocks_key_ptr);

    if (blocks_list_ptr == NULL) {
        Tcl_FreeParse(&parse);
        SetResult("error getting blocks from codearr");
        return TCL_ERROR;
    }

    Tcl_DString ds;
    Tcl_DStringInit(&ds);

    if (TCL_OK != thtml_TclCompileQuotedString(interp, blocks_list_ptr, &ds, &parse)) {
        Tcl_FreeParse(&parse);
        Tcl_DStringFree(&ds);
        return TCL_ERROR;
    }

    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

static int thtml_TclCompileTemplateTextCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr, "TclCompileTemplateTextCmd\n"));

    CheckArgs(3, 3, 1, "codearrVar text");

    int text_length;
    char *text = Tcl_GetStringFromObj(objv[2], &text_length);

    Tcl_Parse parse;
    if (TCL_OK != Tcl_ParseQuotedString(interp, text, text_length, &parse, 0, NULL)) {
        Tcl_FreeParse(&parse);
        return TCL_ERROR;
    }

    Tcl_Obj *blocks_key_ptr = Tcl_NewStringObj("blocks", 6);
    Tcl_IncrRefCount(blocks_key_ptr);
    Tcl_Obj *blocks_list_ptr = Tcl_ObjGetVar2(interp, objv[1], blocks_key_ptr, TCL_LEAVE_ERR_MSG);
    Tcl_DecrRefCount(blocks_key_ptr);

    if (blocks_list_ptr == NULL) {
        Tcl_FreeParse(&parse);
        SetResult("error getting blocks from codearr");
        return TCL_ERROR;
    }

    Tcl_DString ds;
    Tcl_DStringInit(&ds);

    if (TCL_OK != thtml_TclCompileTemplateText(interp, blocks_list_ptr, &ds, &parse)) {
        Tcl_FreeParse(&parse);
        Tcl_DStringFree(&ds);
        return TCL_ERROR;
    }

    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

static int thtml_Md5Cmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr,"Md5Cmd\n"));

    CheckArgs(2,2,1,"text");

    int text_length;
    char *text = Tcl_GetStringFromObj(objv[1], &text_length);

    unsigned char result[16];
    md5String(text, result);
    char hex[33];
    for (int i = 0; i < 16; i++) {
        sprintf(hex + (i * 2), "%02x", result[i]);
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(hex, 32));
    return TCL_OK;
}


static void thtml_ExitHandler(ClientData unused) {
}


void thtml_InitModule() {
    if (!thtml_ModuleInitialized) {
        Tcl_CreateThreadExitHandler(thtml_ExitHandler, NULL);
        thtml_ModuleInitialized = 1;
    }
}

int Thtml_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.6", 0) == NULL) {
        return TCL_ERROR;
    }

    thtml_InitModule();

    Tcl_CreateNamespace(interp, "::thmtl", NULL, NULL);

    Tcl_CreateNamespace(interp, "::thmtl::compiler", NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::compiler::tcl_transform", thtml_TclTransformCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::compiler::tcl_compile_expr", thtml_TclCompileExprCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::compiler::tcl_compile_quoted_string", thtml_TclCompileQuotedStringCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::compiler::tcl_compile_template_text", thtml_TclCompileTemplateTextCmd, NULL, NULL);

    Tcl_CreateNamespace(interp, "::thmtl::util", NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::util::md5", thtml_Md5Cmd, NULL, NULL);

    return Tcl_PkgProvide(interp, "thtml", XSTR(PROJECT_VERSION));
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Thtml_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
