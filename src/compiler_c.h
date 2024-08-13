/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#ifndef THTML_COMPILER_C_H
#define THTML_COMPILER_C_H

#include "common.h"

int thtml_CTransformCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]);
int thtml_CCompileExprCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]);
int thtml_CCompileQuotedStringCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]);
int thtml_CCompileQuotedArgCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]);
int thtml_CCompileTemplateTextCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]);
int thtml_CCompileScriptCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]);
int thtml_CCompileForeachListCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]);

int thtml_CCompileExpr(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr, const char *name);
int thtml_CCompileTemplateText(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr);

#endif //THTML_COMPILER_C_H
