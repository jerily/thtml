/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "expr.h"
#include <ctype.h>
#include <string.h>

static int
thtml_TclAppendExpr_Token(Tcl_Interp *interp, Tcl_Obj *blocks_list_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                          int i);

static int thtml_TclAppendExpr_Variable_Simple(Tcl_Interp *interp, Tcl_DString *ds_ptr, const char *varname_first_part,
                                               int varname_first_part_length) {
    Tcl_DStringAppend(ds_ptr, "$", 1);
    Tcl_DStringAppend(ds_ptr, varname_first_part, varname_first_part_length);
    return TCL_OK;
}

static int
thtml_TclAppendExpr_Variable_Dict(Tcl_Interp *interp, Tcl_DString *ds_ptr, const char *varname_first_part,
                                  int varname_first_part_length, Tcl_Obj **parts,
                                  int num_parts) {
    Tcl_DStringAppend(ds_ptr, "[dict get $", 11);
    Tcl_DStringAppend(ds_ptr, varname_first_part, varname_first_part_length);
    Tcl_DStringAppend(ds_ptr, " {", 2);
    int first = 1;
    for (int i = 0; i < num_parts; i++) {
        if (first) {
            first = 0;
        } else {
            Tcl_DStringAppend(ds_ptr, " ", 1);
        }
        Tcl_Obj *part_ptr = parts[i];
        int part_length;
        const char *part = Tcl_GetStringFromObj(part_ptr, &part_length);
        Tcl_DStringAppend(ds_ptr, part, part_length);
    }
    Tcl_DStringAppend(ds_ptr, "}]", 2);
    return TCL_OK;
}

static int
thtml_TclAppendExpr_Variable(Tcl_Interp *interp, Tcl_Obj *blocks_list_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                             int i) {
    Tcl_Token *token = &parse_ptr->tokenPtr[i];
    int numComponents = token->numComponents;
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
        int num_parts;
        Tcl_Obj **parts;
        if (TCL_OK != Tcl_ListObjGetElements(interp, parts_ptr, &num_parts, &parts)) {
            Tcl_DecrRefCount(parts_ptr);
            return TCL_ERROR;
        }

        // iterate "blocks_list_ptr"
        int num_blocks;
        Tcl_Obj **blocks;
        if (TCL_OK != Tcl_ListObjGetElements(interp, blocks_list_ptr, &num_blocks, &blocks)) {
            Tcl_DecrRefCount(parts_ptr);
            return TCL_ERROR;
        }

        // for each block, get the "variables" list from the dictionary
        for (int j = 0; j < num_blocks; j++) {
            Tcl_Obj *block_ptr = blocks[j];

            Tcl_Obj *variables_key_ptr = Tcl_NewStringObj("variables", -1);
            Tcl_IncrRefCount(variables_key_ptr);
            Tcl_Obj *variables_ptr = NULL;
            if (TCL_OK != Tcl_DictObjGet(interp, block_ptr, variables_key_ptr, &variables_ptr)) {
                Tcl_DecrRefCount(variables_key_ptr);
                Tcl_DecrRefCount(parts_ptr);
                return TCL_ERROR;
            }
            Tcl_DecrRefCount(variables_key_ptr);

            if (variables_ptr == NULL) {
                continue;
            }

            // get the first part of the variable name
            Tcl_Obj *varname_first_part_ptr = parts[0];

            // for each "block_varname_ptr" in "variables_ptr", compare it with "varname_first_part_ptr"
            int num_variables;
            Tcl_Obj **variables;
            if (TCL_OK != Tcl_ListObjGetElements(interp, variables_ptr, &num_variables, &variables)) {
                Tcl_DecrRefCount(parts_ptr);
                return TCL_ERROR;
            }

            int varname_first_part_length;
            const char *varname_first_part = Tcl_GetStringFromObj(varname_first_part_ptr, &varname_first_part_length);
            for (int k = 0; k < num_variables; k++) {
                Tcl_Obj *block_varname_ptr = variables[k];
                int block_varname_length;
                char *block_varname = Tcl_GetStringFromObj(block_varname_ptr, &block_varname_length);
                if (block_varname_length == varname_first_part_length &&
                    0 == strncmp(block_varname, varname_first_part, block_varname_length)) {
                    // found a match
                    if (num_parts == 1) {
                        if (TCL_OK != thtml_TclAppendExpr_Variable_Simple(interp, ds_ptr, varname_first_part,
                                                                          varname_first_part_length)) {
                            Tcl_DecrRefCount(parts_ptr);
                            return TCL_ERROR;
                        }
                    } else {
                        if (TCL_OK !=
                            thtml_TclAppendExpr_Variable_Dict(interp, ds_ptr, varname_first_part,
                                                              varname_first_part_length, parts, num_parts)) {
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
            thtml_TclAppendExpr_Variable_Dict(interp, ds_ptr, "__data__", 8, parts, num_parts)) {
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

static int
thtml_TclAppendExpr_Operator(Tcl_Interp *interp, Tcl_Obj *blocks_list_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                             int i) {
    Tcl_Token *subexpr_token = &parse_ptr->tokenPtr[i];
    Tcl_Token *operator_token = &parse_ptr->tokenPtr[i + 1];
    int operands_offset = i + 2;

    // The numComponents field for a TCL_TOKEN_OPERATOR token is always 0
    // So, we get numComponents from the TCL_TOKEN_SUB_EXPR token that precedes it
    int num_components = subexpr_token->numComponents;

    int num_operands = 0;
    int j = 0;
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
        if (num_operands == 1 && (ch == '!' || ch == '-' || ch == '+')) {
            // one operand

            Tcl_DStringAppend(ds_ptr, operator_token->start, operator_token->size);
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, operands_offset)) {
                return TCL_ERROR;
            }

        } else if (num_operands == 2 &&
                   (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' || ch == '^' || ch == '&' ||
                    ch == '|' ||
                    ch == '~' || ch == '<' || ch == '>' || ch == '=')) {
            // two operands
            Tcl_Token *first_operand = &parse_ptr->tokenPtr[operands_offset];
            // fprintf(stderr, "first_operand of op: %c has numComponents: %d\n", ch, first_operand->numComponents);
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, operands_offset)) {
                return TCL_ERROR;
            }
            Tcl_DStringAppend(ds_ptr, " ", 1);
            Tcl_DStringAppend(ds_ptr, operator_token->start, operator_token->size);
            Tcl_DStringAppend(ds_ptr, " ", 1);
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr,
                                      operands_offset + first_operand->numComponents + 1)) {
                return TCL_ERROR;
            }

        } else if (ch == '?') {
            // three operands

            Tcl_Token *first_operand = &parse_ptr->tokenPtr[operands_offset];
            int second_operand_index = operands_offset + first_operand->numComponents + 1;
            Tcl_Token *second_operand = &parse_ptr->tokenPtr[second_operand_index];
            int third_operand_index = second_operand_index + second_operand->numComponents + 1;

            // fprintf(stderr, "first_operand of op: %c has numComponents: %d\n", ch, first_operand->numComponents);
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, operands_offset)) {
                return TCL_ERROR;
            }
            Tcl_DStringAppend(ds_ptr, " ", 1);
            Tcl_DStringAppend(ds_ptr, operator_token->start, operator_token->size);
            Tcl_DStringAppend(ds_ptr, " ", 1);
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr,second_operand_index)) {
                return TCL_ERROR;
            }
            Tcl_DStringAppend(ds_ptr, " : ", 3);
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, third_operand_index)) {
                return TCL_ERROR;
            }
        } else {
            SetResult("error parsing expression: unsupported operator");
            return TCL_ERROR;
        }
    } else if (operator_token->size == 2) {
        char ch1 = operator_token->start[0];
        char ch2 = operator_token->start[1];
        if ((ch1 == '&' && ch2 == '&') || (ch1 == '|' && ch2 == '|') || (ch1 == '<' && ch2 == '<') ||
            (ch1 == '>' && ch2 == '>') || (ch1 == '=' && ch2 == '=') || (ch1 == '!' && ch2 == '=') ||
                (ch1 == 'e' && ch2 == 'q') || (ch1 == 'n' && ch2 == 'e') ||
                (ch1 == 'i' && ch2 == 'n') || (ch1 == 'n' && ch2 == 'i')) {
            // two operands

            Tcl_Token *first_operand = &parse_ptr->tokenPtr[operands_offset];
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, operands_offset)) {
                return TCL_ERROR;
            }
            Tcl_DStringAppend(ds_ptr, " ", 1);
            Tcl_DStringAppend(ds_ptr, operator_token->start, operator_token->size);
            Tcl_DStringAppend(ds_ptr, " ", 1);
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, operands_offset + first_operand->numComponents + 1)) {
                return TCL_ERROR;
            }

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
thtml_TclAppendExpr_Token(Tcl_Interp *interp, Tcl_Obj *blocks_list_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                          int i) {
    Tcl_Token *token = &parse_ptr->tokenPtr[i];
    if (token->type == TCL_TOKEN_SUB_EXPR) {
        Tcl_Token *next_token = &parse_ptr->tokenPtr[i + 1];
        if (next_token->type == TCL_TOKEN_OPERATOR) {
            // If the first sub-token after the TCL_TOKEN_SUB_EXPR token is a TCL_TOKEN_OPERATOR token,
            // the subexpression consists of an operator and its token operands.
            return thtml_TclAppendExpr_Operator(interp, blocks_list_ptr, ds_ptr, parse_ptr, i);
        } else {
            // Otherwise, the subexpression is a value described by one of the token types TCL_TOKEN_WORD,
            // TCL_TOKEN_TEXT, TCL_TOKEN_BS, TCL_TOKEN_COMMAND, TCL_TOKEN_VARIABLE, and TCL_TOKEN_SUB_EXPR.
            return thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, i + 1);
        }
    } else if (token->type == TCL_TOKEN_VARIABLE) {
        return thtml_TclAppendExpr_Variable(interp, blocks_list_ptr, ds_ptr, parse_ptr, i);
    } else if (token->type == TCL_TOKEN_TEXT) {
        Tcl_DStringAppend(ds_ptr, token->start, token->size);
    } else if (token->type == TCL_TOKEN_COMMAND) {
        SetResult("error parsing expression: command substitution not supported");
        return TCL_ERROR;
    } else if (token->type == TCL_TOKEN_EXPAND_WORD) {
        // todo
    } else if (token->type == TCL_TOKEN_WORD) {
        Tcl_DStringAppend(ds_ptr, "\"", 1);
        for (int j = 0; j < token->numComponents; j++) {
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, i + 1 + j)) {
                return TCL_ERROR;
            }
        }
        Tcl_DStringAppend(ds_ptr, "\"", 1);
    } else if (token->type == TCL_TOKEN_BS) {
        Tcl_DStringAppend(ds_ptr, token->start, token->size);
    }
    return TCL_OK;
}

int thtml_TclCompileExpr(Tcl_Interp *interp, Tcl_Obj *blocks_list_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr) {
    // After Tcl_ParseExpr returns, the first token pointed to by the tokenPtr field of the Tcl_Parse structure
    // always has type TCL_TOKEN_SUB_EXPR. It is followed by the sub-tokens that must be evaluated to produce
    // the value of the expression. Only the token information in the Tcl_Parse structure is modified:
    // the commentStart, commentSize, commandStart, and commandSize fields are not modified by Tcl_ParseExpr.
    if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, 0)) {
        return TCL_ERROR;
    }
    return TCL_OK;
}