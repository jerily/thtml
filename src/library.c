/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "common.h"
#include "library.h"
#include "expr.h"

#include <stdio.h>
#include <string.h>

static int           thtml_ModuleInitialized;



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

    return Tcl_PkgProvide(interp, "thtml", XSTR(PROJECT_VERSION));
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Thtml_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
