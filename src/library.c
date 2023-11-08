/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "common.h"
#include "library.h"
#include "expr.h"
#include "md5.h"

#include <stdio.h>
#include <string.h>

static int           thtml_ModuleInitialized;

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
    Tcl_CreateObjCommand(interp, "::thtml::parse_expr", thtml_ParseExprCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::thtml::md5", thtml_Md5Cmd, NULL, NULL);

    return Tcl_PkgProvide(interp, "thtml", XSTR(PROJECT_VERSION));
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Thtml_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
