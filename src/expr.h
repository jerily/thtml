/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#ifndef THTML_EXPR_H
#define THTML_EXPR_H

#include "common.h"

int thtml_TclCompileExpr(Tcl_Interp *interp, Tcl_DString *dsPtr, Tcl_Parse *parsePtr);

#endif //THTML_EXPR_H
