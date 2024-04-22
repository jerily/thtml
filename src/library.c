/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "common.h"
#include "library.h"
#include "compiler_tcl.h"
#include "compiler_c.h"
#include "md5.h"

#include <stdio.h>
#include <string.h>

static int           thtml_ModuleInitialized;

static int thtml_Md5Cmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr,"Md5Cmd\n"));

    CheckArgs(2,2,1,"text");

    Tcl_Size text_length;
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

    int major, minor, patchLevel, type;
    Tcl_GetVersion(&major, &minor, &patchLevel, &type);

    const char *version = major == 9 ? "9.0" : "8.6";
    if (Tcl_InitStubs(interp, version, 0) == NULL) {
        return TCL_ERROR;
    }

    thtml_InitModule();

    Tcl_CreateNamespace(interp, "::thmtl", NULL, NULL);
    Tcl_CreateNamespace(interp, "::thmtl::compiler", NULL, NULL);

    Tcl_CreateObjCommand(interp, "::thtml::compiler::tcl_transform", thtml_TclTransformCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::compiler::tcl_compile_expr", thtml_TclCompileExprCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::compiler::tcl_compile_quoted_string", thtml_TclCompileQuotedStringCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::compiler::tcl_compile_template_text", thtml_TclCompileTemplateTextCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::compiler::tcl_compile_script", thtml_TclCompileScriptCmd, NULL, NULL);

    Tcl_CreateObjCommand(interp, "::thtml::compiler::c_transform", thtml_CTransformCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::compiler::c_compile_expr", thtml_CCompileExprCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::compiler::c_compile_quoted_string", thtml_CCompileQuotedStringCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::compiler::c_compile_template_text", thtml_CCompileTemplateTextCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::compiler::c_compile_script", thtml_CCompileScriptCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::compiler::c_compile_foreach_list", thtml_CCompileForeachListCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::compiler::c_compile_quoted_arg", thtml_CCompileQuotedArgCmd, NULL, NULL);

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
