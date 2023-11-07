/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "expr.h"
#include <ctype.h>

typedef struct {
} thtml_parse_t;

static int thtml_ParseExpr(Tcl_Interp *interp, const char text, int text_length, thtml_parse_t *parse_ptr) {
    DBG(fprintf(stderr,"ParseExpr\n"));

    // TODO: Implement this function.

    return TCL_OK;
}

int thtml_ParseExprCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr,"ParseExprCmd\n"));

    CheckArgs(2,2,1,"text");

    int text_length;
    char *text = Tcl_GetStringFromObj(objv[1], &text_length);

    thtml_parse_t parse;
    if (TCL_OK != thtml_ParseExpr(interp, text, text_length, &parse)) {
        return TCL_ERROR;
    }

    return TCL_OK;
}
