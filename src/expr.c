/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "expr.h"
#include <ctype.h>

static int thtml_TclAppendExprToken(Tcl_Interp *interp, Tcl_DString *dsPtr, Tcl_Parse *parsePtr, int i);

static int thtml_TclAppendExpr_Variable(Tcl_Interp *interp, Tcl_DString *dsPtr, Tcl_Parse *parsePtr, int i) {
    Tcl_Token *token = &parsePtr->tokenPtr[i];
    int numComponents = token->numComponents;
    if (numComponents == 1) {
        Tcl_Token *text_token = &parsePtr->tokenPtr[i + 1];
        // todo: split varname into parts by "." and decide whether it is a simple var or a dict reference
        Tcl_DStringAppend(dsPtr, "$", 1);
        Tcl_DStringAppend(dsPtr, text_token->start, text_token->size);
    } else {
        SetResult("error parsing expression: array variables not supported");
        return TCL_ERROR;
    }
    return TCL_OK;
}

static int thtml_TclAppendExpr_Operator(Tcl_Interp *interp, Tcl_DString *dsPtr, Tcl_Parse *parsePtr, int i) {
    Tcl_Token *subexpr_token = &parsePtr->tokenPtr[i];
    Tcl_Token *operator_token = &parsePtr->tokenPtr[i + 1];

    // The numComponents field for a TCL_TOKEN_OPERATOR token is always 0
    // So, we get numComponents from the TCL_TOKEN_SUB_EXPR token that precedes it
    int num_components = subexpr_token->numComponents;

    int num_operands = 0;
    int j = 0;
    while (j < num_components - 2) {
        Tcl_Token *operand_token = &parsePtr->tokenPtr[i + 2 + j];
        if (operand_token->type == TCL_TOKEN_SUB_EXPR) {
            fprintf(stderr, ">>> subexpr %d\n", operand_token->numComponents);
            num_operands++;
            j += 1 + operand_token->numComponents;
            continue;
        }
        fprintf(stderr, "num_operands: %d j: %d num_components: %d\n", num_operands, j, num_components);
        // the following should never happen
        SetResult("error parsing expression: not enough operands");
        return TCL_ERROR;
    }


    if (operator_token->size == 1) {
        char ch = operator_token->start[0];
        if (num_operands == 1 && (ch == '!' || ch == '-' || ch == '+')) {
            // one operand
            // todo: implement

        } else if (num_operands == 2 && (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' || ch == '^' || ch == '&' || ch == '|' ||
            ch == '~' || ch == '<' || ch == '>' || ch == '=')) {
            // two operands

            Tcl_Token *first_operand = &parsePtr->tokenPtr[i + 2];
            thtml_TclAppendExprToken(interp, dsPtr, parsePtr, i + 2);
            Tcl_DStringAppend(dsPtr, " ", 1);
            Tcl_DStringAppend(dsPtr, operator_token->start, operator_token->size);
            Tcl_DStringAppend(dsPtr, " ", 1);
            thtml_TclAppendExprToken(interp, dsPtr, parsePtr, i + 2 + first_operand->numComponents);

        } else if (ch == '?') {
            // three operands
            // todo: implement
        } else {
            SetResult("error parsing expression: unsupported operator");
            return TCL_ERROR;
        }
    } else if (operator_token->size == 2) {
        char ch1 = operator_token->start[0];
        char ch2 = operator_token->start[1];
        if ((ch1 == '&' && ch2 == '&') || (ch1 == '|' && ch2 == '|') || (ch1 == '<' && ch2 == '<') ||
            (ch1 == '>' && ch2 == '>') || (ch1 == '=' && ch2 == '=') || (ch1 == '!' && ch2 == '=')) {
            // two operands

            Tcl_Token *first_operand = &parsePtr->tokenPtr[i + 2];
            thtml_TclAppendExprToken(interp, dsPtr, parsePtr, i + 2);
            Tcl_DStringAppend(dsPtr, " ", 1);
            Tcl_DStringAppend(dsPtr, operator_token->start, operator_token->size);
            Tcl_DStringAppend(dsPtr, " ", 1);
            thtml_TclAppendExprToken(interp, dsPtr, parsePtr, i + 2 + first_operand->numComponents);

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

static int thtml_TclAppendExprToken(Tcl_Interp *interp, Tcl_DString *dsPtr, Tcl_Parse *parsePtr, int i) {
    Tcl_Token *token = &parsePtr->tokenPtr[i];
    if (token->type == TCL_TOKEN_SUB_EXPR) {
        fprintf(stderr, "subexpr %d\n", token->numComponents);
        Tcl_Token *next_token = &parsePtr->tokenPtr[i + 1];
        if (next_token->type == TCL_TOKEN_OPERATOR) {
            return thtml_TclAppendExpr_Operator(interp, dsPtr, parsePtr, i);
        } else {
            return TCL_OK;
        }
    } else if (token->type == TCL_TOKEN_VARIABLE) {
        return thtml_TclAppendExpr_Variable(interp, dsPtr, parsePtr, i);
    } else if (token->type == TCL_TOKEN_TEXT) {
        Tcl_DStringAppend(dsPtr, token->start, token->size);
        return TCL_OK;
    } else if (token->type == TCL_TOKEN_COMMAND) {
        // todo
    } else if (token->type == TCL_TOKEN_EXPAND_WORD) {
        // todo
    } else if (token->type == TCL_TOKEN_WORD) {
        // todo
    } else if (token->type == TCL_TOKEN_BS) {
        // todo
    }
    return TCL_OK;
}

int thtml_TclCompileExpr(Tcl_Interp *interp, Tcl_DString *dsPtr, Tcl_Parse *parsePtr) {
    // After Tcl_ParseExpr returns, the first token pointed to by the tokenPtr field of the Tcl_Parse structure
    // always has type TCL_TOKEN_SUB_EXPR. It is followed by the sub-tokens that must be evaluated to produce
    // the value of the expression. Only the token information in the Tcl_Parse structure is modified:
    // the commentStart, commentSize, commandStart, and commandSize fields are not modified by Tcl_ParseExpr.
    if (TCL_OK != thtml_TclAppendExprToken(interp, dsPtr, parsePtr, 0)) {
        return TCL_ERROR;
    }
    return TCL_OK;
}