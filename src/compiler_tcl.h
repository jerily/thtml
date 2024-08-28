/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#ifndef THTML_COMPILER_TCL_H
#define THTML_COMPILER_TCL_H

#include "common.h"

int thtml_TclTransformCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]);
int thtml_TclCompileExprCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]);
int thtml_TclCompileQuotedStringCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]);
int thtml_TclCompileTemplateTextCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]);
int thtml_TclCompileScriptCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]);
int thtml_TclCompileForeachListCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]);

int thtml_TclCompileExpr(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr, const char *name);
int thtml_TclCompileQuotedString(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr, const char *name);
int thtml_TclCompileTemplateText(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr);
int thtml_TclCompileCommand(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr, const char *name, int nested);
int thtml_TclAppendVariable(Tcl_Interp *interp, Tcl_Obj *codearrVar_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr, Tcl_Size i, const char *name, Tcl_DString *cmd_ds_ptr, int in_eval_p);

#endif //THTML_COMPILER_TCL_H
