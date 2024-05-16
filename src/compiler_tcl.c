/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "compiler_tcl.h"
#include <ctype.h>
#include <string.h>
#include <assert.h>

static int subcmd_count = 0;
static int template_cmd_count = 0;

static int thtml_TclAppendCommand_Token(Tcl_Interp *interp, Tcl_Obj *blocks_list_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                                 Tcl_Size i, Tcl_Size *out_i, const char *name, Tcl_DString *cmd_ds_ptr, int in_eval_p);

int thtml_TclTransformCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr, "TclTransformCmd\n"));

    CheckArgs(2,2,1,"intermediate_code");

    Tcl_Size intermediate_code_length;
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
                Tcl_DStringAppend(&ds, "\nappend __ds_default__ \"", -1);
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

int thtml_TclCompileExprCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr, "TclCompileExprCmd\n"));

    CheckArgs(4, 4, 1, "codearrVar text name");

    Tcl_Size text_length;
    char *text = Tcl_GetStringFromObj(objv[2], &text_length);

    Tcl_Size name_length;
    char *name = Tcl_GetStringFromObj(objv[3], &name_length);

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

    if (TCL_OK != thtml_TclCompileExpr(interp, blocks_list_ptr, &ds, &parse, name)) {
        Tcl_FreeParse(&parse);
        Tcl_DStringFree(&ds);
        return TCL_ERROR;
    }

    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

int thtml_TclCompileQuotedStringCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr, "TclCompileQuotedStringCmd\n"));

    CheckArgs(3, 4, 1, "codearrVar text ?name?");

    Tcl_Size text_length;
    char *text = Tcl_GetStringFromObj(objv[2], &text_length);

    const char *name = "default";
    if (objc == 4) {
        name = Tcl_GetString(objv[3]);
    }

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

    if (TCL_OK != thtml_TclCompileQuotedString(interp, blocks_list_ptr, &ds, &parse, name)) {
        Tcl_FreeParse(&parse);
        Tcl_DStringFree(&ds);
        return TCL_ERROR;
    }

    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

int thtml_TclCompileTemplateTextCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr, "TclCompileTemplateTextCmd\n"));

    CheckArgs(3, 3, 1, "codearrVar text");

    Tcl_Size text_length;
    char *text = Tcl_GetStringFromObj(objv[2], &text_length);

    const char *p = text;
    const char *end = text + text_length;

    Tcl_DString text_ds;
    Tcl_DStringInit(&text_ds);
    int count = 0;
    while (p < end) {
        if (*p == '[') {
            count++;
            Tcl_DStringAppend(&text_ds, "[", 1);
        } else if (*p == ']') {
            count--;
            Tcl_DStringAppend(&text_ds, "]", 1);
        } else if (p > text && p < end - 1 && *p == '"' && count == 0) {
            Tcl_DStringAppend(&text_ds, "\\\"", 2);
        } else if (p > text && p < end - 1 && *p == '\\' && count == 0) {
            if (p + 1 < end - 1 && *(p + 1) == '[') {
                Tcl_DStringAppend(&text_ds, "\\[", 2);
                p+=2;
                continue;
            } else if (p + 1 < end - 1 && *(p + 1) == ']') {
                Tcl_DStringAppend(&text_ds, "\\]", 2);
                p+=2;
                continue;
            } else if (p + 1 < end - 1 && *(p + 1) == '"') {
                // do nothing, we escape double quotes when we see them
            } else {
                Tcl_DStringAppend(&text_ds, "\\", 1);
            }
        } else {
            Tcl_DStringAppend(&text_ds, p, 1);
        }
        p++;
    }

    Tcl_Parse parse;
    if (TCL_OK != Tcl_ParseQuotedString(interp, Tcl_DStringValue(&text_ds), Tcl_DStringLength(&text_ds), &parse, 0, NULL)) {
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

int thtml_TclCompileScriptCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr, "TclCompileScriptCmd\n"));

    CheckArgs(4, 4, 1, "codearrVar text name");

    Tcl_Size text_length;
    char *text = Tcl_GetStringFromObj(objv[2], &text_length);

    Tcl_Size name_length;
    const char *name = Tcl_GetStringFromObj(objv[3], &name_length);

    Tcl_Obj *blocks_key_ptr = Tcl_NewStringObj("blocks", 6);
    Tcl_IncrRefCount(blocks_key_ptr);
    Tcl_Obj *blocks_list_ptr = Tcl_ObjGetVar2(interp, objv[1], blocks_key_ptr, TCL_LEAVE_ERR_MSG);
    Tcl_DecrRefCount(blocks_key_ptr);

    if (blocks_list_ptr == NULL) {
        SetResult("error getting blocks from codearr");
        return TCL_ERROR;
    }

    Tcl_DString ds;
    Tcl_DStringInit(&ds);

    Tcl_Parse parse;
    if (TCL_OK != Tcl_ParseCommand(interp, text, text_length, 0, &parse)) {
        Tcl_FreeParse(&parse);
        return TCL_ERROR;
    }

    if (TCL_OK != thtml_TclCompileCommand(interp, blocks_list_ptr, &ds, &parse, name, 0)) {
        Tcl_FreeParse(&parse);
        Tcl_DStringFree(&ds);
        return TCL_ERROR;
    }
    Tcl_DStringAppend(&ds, "\nset __", -1);
    Tcl_DStringAppend(&ds, name, name_length);
    Tcl_DStringAppend(&ds, "__ $__ds_", -1);
    Tcl_DStringAppend(&ds, name, name_length);
    Tcl_DStringAppend(&ds, "__", -1);

    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);
    return TCL_OK;
}


static int
thtml_TclAppendExpr_Token(Tcl_Interp *interp, Tcl_Obj *blocks_list_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                          Tcl_Size i, const char *name, Tcl_DString *expr_ds_ptr);

static int thtml_TclAppendVariable_Simple(Tcl_Interp *interp, Tcl_DString *ds_ptr, const char *varname_first_part,
                                          Tcl_Size varname_first_part_length, const char *name, Tcl_DString *cmd_ds_ptr, int in_eval_p) {
    if (cmd_ds_ptr == NULL) {
        // Tcl_DStringAppend(__ds_default__, "\"$x\"", -1);
        Tcl_DStringAppend(ds_ptr, "\nappend __ds_", -1);
        Tcl_DStringAppend(ds_ptr, name, -1);
        Tcl_DStringAppend(ds_ptr, "__ $", -1);
        Tcl_DStringAppend(ds_ptr, varname_first_part, varname_first_part_length);
    } else {
        if (in_eval_p) {
            Tcl_DStringAppend(cmd_ds_ptr, "IN-EVAL: $", -1);
            Tcl_DStringAppend(cmd_ds_ptr, varname_first_part, varname_first_part_length);
        } else {
            Tcl_DStringAppend(cmd_ds_ptr, "$", -1);
            Tcl_DStringAppend(cmd_ds_ptr, varname_first_part, varname_first_part_length);
        }
    }
    return TCL_OK;
}

static int
thtml_TclAppendVariable_Dict(Tcl_Interp *interp, Tcl_DString *ds_ptr, const char *varname_first_part,
                             Tcl_Size varname_first_part_length, Tcl_Obj **parts,
                             Tcl_Size num_parts, const char *name, Tcl_DString *cmd_ds_ptr, int in_eval_p) {
    static int count_var_dict_subst = 0;
    count_var_dict_subst++;
    char count_var_dict_subst_str[12];
    snprintf(count_var_dict_subst_str, 12, "%d", count_var_dict_subst);

    int first = 1;
    for (int i = 0; i < num_parts; i++) {
        if (first) {
            first = 0;
        } else {
            Tcl_DStringAppend(ds_ptr, " ", 1);
        }
        Tcl_Obj *part_ptr = parts[i];
        Tcl_Size part_length;
        const char *part = Tcl_GetStringFromObj(part_ptr, &part_length);

        Tcl_DStringAppend(ds_ptr, "\nset __", -1);
        Tcl_DStringAppend(ds_ptr, part, part_length);
        Tcl_DStringAppend(ds_ptr, count_var_dict_subst_str, -1);
        Tcl_DStringAppend(ds_ptr, "__ ", -1);
        Tcl_DStringAppend(ds_ptr, "[dict get $", -1);
        // TODO: this is a chain of keys, we need to get the name of the previous part in the chain
        Tcl_DStringAppend(ds_ptr, varname_first_part, varname_first_part_length);
        Tcl_DStringAppend(ds_ptr, " ", -1);
        Tcl_DStringAppend(ds_ptr, part, part_length);
        Tcl_DStringAppend(ds_ptr, "]", -1);

        if (cmd_ds_ptr == NULL) {
            if (in_eval_p) {
                Tcl_DStringAppend(ds_ptr, "\nappend __ds_", -1);
                Tcl_DStringAppend(ds_ptr, name, -1);
                Tcl_DStringAppend(ds_ptr, "__ \\{$__", -1);
                Tcl_DStringAppend(ds_ptr, part, part_length);
                Tcl_DStringAppend(ds_ptr, count_var_dict_subst_str, -1);
                Tcl_DStringAppend(ds_ptr, "__\\}", -1);
            } else {
                Tcl_DStringAppend(ds_ptr, "\nappend __ds_", -1);
                Tcl_DStringAppend(ds_ptr, name, -1);
                Tcl_DStringAppend(ds_ptr, "__ $__", -1);
                Tcl_DStringAppend(ds_ptr, part, part_length);
                Tcl_DStringAppend(ds_ptr, count_var_dict_subst_str, -1);
                Tcl_DStringAppend(ds_ptr, "__", -1);
            }

        } else {
            Tcl_DStringAppend(cmd_ds_ptr, "$__", -1);
            Tcl_DStringAppend(cmd_ds_ptr, part, part_length);
            Tcl_DStringAppend(cmd_ds_ptr, count_var_dict_subst_str, -1);
            Tcl_DStringAppend(cmd_ds_ptr, "__", -1);
        }
    }
    return TCL_OK;
}

int
thtml_TclAppendVariable(Tcl_Interp *interp, Tcl_Obj *blocks_list_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                        Tcl_Size i, const char *name, Tcl_DString *cmd_ds_ptr, int in_eval_p) {
    Tcl_Token *token = &parse_ptr->tokenPtr[i];
    Tcl_Size numComponents = token->numComponents;
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
        Tcl_Size num_parts;
        Tcl_Obj **parts;
        if (TCL_OK != Tcl_ListObjGetElements(interp, parts_ptr, &num_parts, &parts)) {
            Tcl_DecrRefCount(parts_ptr);
            return TCL_ERROR;
        }

        // iterate "blocks_list_ptr"
        Tcl_Size num_blocks;
        Tcl_Obj **blocks;
        if (TCL_OK != Tcl_ListObjGetElements(interp, blocks_list_ptr, &num_blocks, &blocks)) {
            Tcl_DecrRefCount(parts_ptr);
            return TCL_ERROR;
        }

        // for each block, get the "variables" list from the dictionary
        for (int j = 0; j < num_blocks; j++) {
            Tcl_Obj *block_ptr = blocks[j];

//            fprintf(stderr, "block: %s\n", Tcl_GetString(block_ptr));

            Tcl_Obj *variables_key_ptr = Tcl_NewStringObj("varnames", -1);
            Tcl_IncrRefCount(variables_key_ptr);
            Tcl_Obj *variables_ptr = NULL;
            if (TCL_OK != Tcl_DictObjGet(interp, block_ptr, variables_key_ptr, &variables_ptr)) {
                Tcl_DecrRefCount(variables_key_ptr);
                Tcl_DecrRefCount(parts_ptr);
                return TCL_ERROR;
            }
            Tcl_DecrRefCount(variables_key_ptr);

            if (variables_ptr == NULL) {
//                fprintf(stderr, "no varnames\n");
                continue;
            }

            // get the first part of the variable name
            Tcl_Obj *varname_first_part_ptr = parts[0];

            // for each "block_varname_ptr" in "variables_ptr", compare it with "varname_first_part_ptr"
            Tcl_Size num_variables;
            Tcl_Obj **variables;
            if (TCL_OK != Tcl_ListObjGetElements(interp, variables_ptr, &num_variables, &variables)) {
                Tcl_DecrRefCount(parts_ptr);
                return TCL_ERROR;
            }

            Tcl_Size varname_first_part_length;
            const char *varname_first_part = Tcl_GetStringFromObj(varname_first_part_ptr, &varname_first_part_length);
            for (int k = 0; k < num_variables; k++) {
                Tcl_Obj *block_varname_ptr = variables[k];
                Tcl_Size block_varname_length;
                char *block_varname = Tcl_GetStringFromObj(block_varname_ptr, &block_varname_length);

//                fprintf(stderr, "block_varname: %s\n", block_varname);
//                fprintf(stderr, "varname_first_part: %s\n", varname_first_part);
//                fprintf(stderr, "----\n");

                if (block_varname_length == varname_first_part_length &&
                    0 == strncmp(block_varname, varname_first_part, block_varname_length)) {
                    // found a match
                    if (num_parts == 1) {
                        if (TCL_OK != thtml_TclAppendVariable_Simple(interp, ds_ptr, varname_first_part,
                                                                     varname_first_part_length, name, cmd_ds_ptr, in_eval_p)) {
                            Tcl_DecrRefCount(parts_ptr);
                            return TCL_ERROR;
                        }
                    } else {
                        if (TCL_OK !=
                                thtml_TclAppendVariable_Dict(interp, ds_ptr, varname_first_part,
                                                             varname_first_part_length, &parts[1], num_parts - 1, name, cmd_ds_ptr, in_eval_p)) {
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
                thtml_TclAppendVariable_Dict(interp, ds_ptr, "__data__", 8, parts, num_parts, name, cmd_ds_ptr, in_eval_p)) {
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
                             Tcl_Size i, const char *name, Tcl_DString *cmd_ds_ptr) {

    Tcl_Token *subexpr_token = &parse_ptr->tokenPtr[i];
    Tcl_Token *operator_token = &parse_ptr->tokenPtr[i + 1];
    Tcl_Size operands_offset = i + 2;

    // The numComponents field for a TCL_TOKEN_OPERATOR token is always 0
    // So, we get numComponents from the TCL_TOKEN_SUB_EXPR token that precedes it
    Tcl_Size num_components = subexpr_token->numComponents;

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
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, operands_offset, name, cmd_ds_ptr)) {
                return TCL_ERROR;
            }

        } else if (num_operands == 2 &&
                   (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' || ch == '^' || ch == '&' ||
                    ch == '|' ||
                    ch == '~' || ch == '<' || ch == '>' || ch == '=')) {
            // two operands
            Tcl_Token *first_operand = &parse_ptr->tokenPtr[operands_offset];
            // fprintf(stderr, "first_operand of op: %c has numComponents: %d\n", ch, first_operand->numComponents);
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, operands_offset, name, cmd_ds_ptr)) {
                return TCL_ERROR;
            }
            Tcl_DStringAppend(cmd_ds_ptr, " ", 1);
            Tcl_DStringAppend(cmd_ds_ptr, operator_token->start, operator_token->size);
            Tcl_DStringAppend(cmd_ds_ptr, " ", 1);
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr,
                                                    operands_offset + first_operand->numComponents + 1, name, cmd_ds_ptr)) {
                return TCL_ERROR;
            }

        } else if (ch == '?') {
            // three operands

            Tcl_Token *first_operand = &parse_ptr->tokenPtr[operands_offset];
            Tcl_Size second_operand_index = operands_offset + first_operand->numComponents + 1;
            Tcl_Token *second_operand = &parse_ptr->tokenPtr[second_operand_index];
            Tcl_Size third_operand_index = second_operand_index + second_operand->numComponents + 1;

            // fprintf(stderr, "first_operand of op: %c has numComponents: %d\n", ch, first_operand->numComponents);
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, operands_offset, name, cmd_ds_ptr)) {
                return TCL_ERROR;
            }
            Tcl_DStringAppend(cmd_ds_ptr, " ", 1);
            Tcl_DStringAppend(cmd_ds_ptr, operator_token->start, operator_token->size);
            Tcl_DStringAppend(cmd_ds_ptr, " ", 1);
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, second_operand_index, name, cmd_ds_ptr)) {
                return TCL_ERROR;
            }
            Tcl_DStringAppend(cmd_ds_ptr, " : ", 3);
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, third_operand_index, name, cmd_ds_ptr)) {
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
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, operands_offset, name, cmd_ds_ptr)) {
                return TCL_ERROR;
            }
            Tcl_DStringAppend(cmd_ds_ptr, " ", 1);
            Tcl_DStringAppend(cmd_ds_ptr, operator_token->start, operator_token->size);
            Tcl_DStringAppend(cmd_ds_ptr, " ", 1);
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr,
                                                    operands_offset + first_operand->numComponents + 1, name, cmd_ds_ptr)) {
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
                          Tcl_Size i, const char *name, Tcl_DString *expr_ds_ptr) {
    Tcl_Token *token = &parse_ptr->tokenPtr[i];
//    Tcl_DStringAppend(expr_ds_ptr, token->start, token->size);
//    Tcl_DStringAppend(expr_ds_ptr, "\n", -1);
    if (token->type == TCL_TOKEN_SUB_EXPR) {
        Tcl_Token *next_token = &parse_ptr->tokenPtr[i + 1];
        Tcl_DStringAppend(expr_ds_ptr, "(", 1);
        if (next_token->type == TCL_TOKEN_OPERATOR) {
            // If the first sub-token after the TCL_TOKEN_SUB_EXPR token is a TCL_TOKEN_OPERATOR token,
            // the subexpression consists of an operator and its token operands.
            if (TCL_OK != thtml_TclAppendExpr_Operator(interp, blocks_list_ptr, ds_ptr, parse_ptr, i, name, expr_ds_ptr)) {
                return TCL_ERROR;
            }
        } else if (next_token->type == TCL_TOKEN_TEXT) {
            Tcl_DStringAppend(expr_ds_ptr, "{", 1);
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, i + 1, name, expr_ds_ptr)) {
                return TCL_ERROR;
            }
            Tcl_DStringAppend(expr_ds_ptr, "}", 1);
        } else {
            // Otherwise, the subexpression is a value described by one of the token types TCL_TOKEN_WORD,
            // TCL_TOKEN_BS, TCL_TOKEN_COMMAND, TCL_TOKEN_VARIABLE, and TCL_TOKEN_SUB_EXPR.
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, i + 1, name, expr_ds_ptr)) {
                return TCL_ERROR;
            }
        }
        Tcl_DStringAppend(expr_ds_ptr, ")", 1);
    } else if (token->type == TCL_TOKEN_VARIABLE) {
        return thtml_TclAppendVariable(interp, blocks_list_ptr, ds_ptr, parse_ptr, i, "default", expr_ds_ptr, 0);
    } else if (token->type == TCL_TOKEN_TEXT) {
        Tcl_DStringAppend(expr_ds_ptr, token->start, token->size);
    } else if (token->type == TCL_TOKEN_COMMAND) {
        int in_eval_p = 0;
        if (TCL_OK != thtml_TclAppendCommand_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, i, &i, name, expr_ds_ptr, in_eval_p)) {
            return TCL_ERROR;
        }
    } else if (token->type == TCL_TOKEN_EXPAND_WORD) {
        SetResult("error parsing expression: expand word not supported");
        return TCL_ERROR;
    } else if (token->type == TCL_TOKEN_WORD) {
        Tcl_DStringAppend(expr_ds_ptr, "\"", 1);
        for (int j = 0; j < token->numComponents; j++) {
            if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, i + 1 + j, name, expr_ds_ptr)) {
                return TCL_ERROR;
            }
        }
        Tcl_DStringAppend(expr_ds_ptr, "\"", 1);
    } else if (token->type == TCL_TOKEN_BS) {
        Tcl_DStringAppend(expr_ds_ptr, token->start, token->size);
    } else {
        fprintf(stderr, "other type of token: %d %*.s\n", token->type, token->size, token->start);
    }
    return TCL_OK;
}

int thtml_TclCompileExpr(Tcl_Interp *interp, Tcl_Obj *blocks_list_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr, const char *name) {
    // After Tcl_ParseExpr returns, the first token pointed to by the tokenPtr field of the Tcl_Parse structure
    // always has type TCL_TOKEN_SUB_EXPR. It is followed by the sub-tokens that must be evaluated to produce
    // the value of the expression. Only the token information in the Tcl_Parse structure is modified:
    // the commentStart, commentSize, commandStart, and commandSize fields are not modified by Tcl_ParseExpr.

    Tcl_DString expr_ds;
    Tcl_DStringInit(&expr_ds);
    Tcl_DStringAppend(&expr_ds, "\nset __", -1);
    Tcl_DStringAppend(&expr_ds, name, -1);
    Tcl_DStringAppend(&expr_ds, "__ [expr {", -1);

    if (TCL_OK != thtml_TclAppendExpr_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, 0, name, &expr_ds)) {
        return TCL_ERROR;
    }
    Tcl_DStringAppend(&expr_ds, "}]", -1);

    Tcl_DStringAppend(ds_ptr, Tcl_DStringValue(&expr_ds), Tcl_DStringLength(&expr_ds));
    Tcl_DStringFree(&expr_ds);


    return TCL_OK;
}

int
thtml_TclCompileQuotedString(Tcl_Interp *interp, Tcl_Obj *blocks_list_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr, const char *name) {
    for (int i = 0; i < parse_ptr->numTokens; i++) {
        Tcl_Token *token = &parse_ptr->tokenPtr[i];
        if (token->type == TCL_TOKEN_TEXT) {
            Tcl_DStringAppend(ds_ptr, "\nappend __ds_", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "__ ", -1);
            Tcl_DStringStartSublist(ds_ptr);
            Tcl_DStringAppend(ds_ptr, token->start, token->size);
            Tcl_DStringEndSublist(ds_ptr);
        } else if (token->type == TCL_TOKEN_BS) {
            Tcl_DStringAppend(ds_ptr, "\nappend __ds_", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "__ ", -1);
            Tcl_DStringStartSublist(ds_ptr);
            Tcl_DStringAppend(ds_ptr, &token->start[1], token->size-1);
            Tcl_DStringEndSublist(ds_ptr);
        } else if (token->type == TCL_TOKEN_COMMAND) {
            SetResult("error parsing quoted string: command substitution not supported");
            return TCL_ERROR;
        } else if (token->type == TCL_TOKEN_VARIABLE) {
            if (TCL_OK != thtml_TclAppendVariable(interp, blocks_list_ptr, ds_ptr, parse_ptr, i, name, NULL, 0)) {
                return TCL_ERROR;
            }
            i++;
        } else {
            fprintf(stderr, "type: %d\n", token->type);
            SetResult("error parsing quoted string: unsupported token type");
            return TCL_ERROR;
        }
    }
    return TCL_OK;
}

int
thtml_TclCompileTemplateText(Tcl_Interp *interp, Tcl_Obj *blocks_list_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr) {
    for (Tcl_Size i = 0; i < parse_ptr->numTokens; i++) {
        Tcl_Token *token = &parse_ptr->tokenPtr[i];
        if (token->type == TCL_TOKEN_TEXT) {
            Tcl_DStringAppend(ds_ptr, token->start, token->size);
        } else if (token->type == TCL_TOKEN_BS) {
//            fprintf(stderr, "TCL_TOKEN_BS: %.*s\n", token->size, token->start);
            Tcl_DStringAppend(ds_ptr, token->start, token->size);
        } else if (token->type == TCL_TOKEN_COMMAND) {

            template_cmd_count++;

            char cmd_name[64];
            snprintf(cmd_name, 64, "cmd%d", template_cmd_count);


            DBG(fprintf(stderr, "CompileTemplateText\n"));
            Tcl_Parse cmd_parse;
            if (TCL_OK != Tcl_ParseCommand(interp, token->start + 1, token->size - 2, 0, &cmd_parse)) {
                Tcl_FreeParse(&cmd_parse);
                return TCL_ERROR;
            }

            Tcl_DStringAppend(ds_ptr, "\x03", -1);
            Tcl_DStringAppend(ds_ptr, "\nset __ds_", -1);
            Tcl_DStringAppend(ds_ptr, cmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "__ {}", -1);

            if (TCL_OK != thtml_TclCompileCommand(interp, blocks_list_ptr, ds_ptr, &cmd_parse, cmd_name, 0)) {
                Tcl_FreeParse(&cmd_parse);
                return TCL_ERROR;
            }

            // puts $__cmd1__
//            Tcl_DStringAppend(ds_ptr, "\nputs $__", -1);
//            Tcl_DStringAppend(ds_ptr, cmd_name, -1);
//            Tcl_DStringAppend(ds_ptr, "__", -1);

            // append __ds_default__ [::thtml::runtime::tcl::evaluate_script $__cmd1__]
            Tcl_DStringAppend(ds_ptr, "\nappend __ds_default__ [::thtml::runtime::tcl::evaluate_script $__", -1);
            Tcl_DStringAppend(ds_ptr, cmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "__]", -1);

            Tcl_DStringAppend(ds_ptr, "\x02", -1);

            Tcl_FreeParse(&cmd_parse);

        } else if (token->type == TCL_TOKEN_VARIABLE) {
            Tcl_DStringAppend(ds_ptr, "\x03", -1);
            if (TCL_OK != thtml_TclAppendVariable(interp, blocks_list_ptr, ds_ptr, parse_ptr, i, "default", NULL, 0)) {
                return TCL_ERROR;
            }
            Tcl_DStringAppend(ds_ptr, "\n\x02", 2);
            i++;
        } else {
            SetResult("error parsing quoted string: unsupported token type");
            return TCL_ERROR;
        }
    }
    return TCL_OK;
}

static int thtml_TclAppendCommand_Token(Tcl_Interp *interp, Tcl_Obj *blocks_list_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr,
                                Tcl_Size i, Tcl_Size *out_i, const char *name, Tcl_DString *cmd_ds_ptr, int in_eval_p) {
    if (i >= parse_ptr->numTokens) {
        return TCL_OK;
    }
    Tcl_Token *token = &parse_ptr->tokenPtr[i];
//     fprintf(stderr, "thtml_TclAppendCommand_Token: i: %d type: %d - token: %.*s - numComponents: %d\n", i, token->type,
//            token->size, token->start, token->numComponents);

    Tcl_DStringAppend(ds_ptr, "\n#AppendCommand_Token: ", -1);
    Tcl_DStringAppend(ds_ptr, token->start, token->size);

    if (token->type == TCL_TOKEN_VARIABLE) {
        if (TCL_OK != thtml_TclAppendVariable(interp, blocks_list_ptr, ds_ptr, parse_ptr, i, name, cmd_ds_ptr, in_eval_p)) {
            return TCL_ERROR;
        }
        *out_i = i + 2;
        return TCL_OK;
    } else if (token->type == TCL_TOKEN_SIMPLE_WORD) {
        Tcl_Token *text_token = &parse_ptr->tokenPtr[i + 1];
        assert(text_token->type == TCL_TOKEN_TEXT);
        Tcl_DStringAppend(ds_ptr, "\nappend __ds_", -1);
        Tcl_DStringAppend(ds_ptr, name, -1);
        Tcl_DStringAppend(ds_ptr, "__ {{", -1);
        Tcl_DStringAppend(ds_ptr, text_token->start, text_token->size);
        Tcl_DStringAppend(ds_ptr, "}}", -1);
        *out_i = i + 2;
        return TCL_OK;
    } else if (token->type == TCL_TOKEN_TEXT) {
        Tcl_DStringAppend(ds_ptr, "\nappend __ds_", -1);
        Tcl_DStringAppend(ds_ptr, name, -1);
        Tcl_DStringAppend(ds_ptr, "__ {", -1);
        Tcl_DStringAppend(ds_ptr, token->start, token->size);
        Tcl_DStringAppend(ds_ptr, "}", -1);
        *out_i = i + 1;
        return TCL_OK;
    } else if (token->type == TCL_TOKEN_COMMAND) {

        subcmd_count++;

        char subcmd_name[64];
        snprintf(subcmd_name, 64, "%s_subcmd%d", name, subcmd_count);

        Tcl_DStringAppend(ds_ptr, "\n#SubCommand: ", -1);
        Tcl_DStringAppend(ds_ptr, token->start, token->size);

        Tcl_DStringAppend(ds_ptr, "\nset __ds_", -1);
        Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
        Tcl_DStringAppend(ds_ptr, "__ {}", -1);

        Tcl_Parse subcmd_parse;
        if (TCL_OK != Tcl_ParseCommand(interp, token->start + 1, token->size - 2, 0, &subcmd_parse)) {
            return TCL_ERROR;
        }
        if (TCL_OK != thtml_TclCompileCommand(interp, blocks_list_ptr, ds_ptr, &subcmd_parse, subcmd_name, 1)) {
            return TCL_ERROR;
        }

        if (cmd_ds_ptr != NULL) {
            Tcl_DStringAppend(cmd_ds_ptr, "[subst $__", -1);
            Tcl_DStringAppend(cmd_ds_ptr, subcmd_name, -1);
            Tcl_DStringAppend(cmd_ds_ptr, "__]", -1);
        } else {
            Tcl_DStringAppend(ds_ptr, "\nappend __ds_", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "__ $__", -1);
            Tcl_DStringAppend(ds_ptr, subcmd_name, -1);
            Tcl_DStringAppend(ds_ptr, "__", -1);
        }

    } else if (token->type == TCL_TOKEN_EXPAND_WORD) {
        SetResult("error parsing expression: expand word not supported");
        return TCL_ERROR;
    } else if (token->type == TCL_TOKEN_WORD) {
        Tcl_Size start_i = i;
        Tcl_Size j = i + 1;
        while (i - start_i < token->numComponents) {
            if (TCL_OK != thtml_TclAppendCommand_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, j, &i, name, cmd_ds_ptr, in_eval_p)) {
                return TCL_ERROR;
            }
            j = i;
        }
        *out_i = i + 1;
        return TCL_OK;
    } else if (token->type == TCL_TOKEN_BS) {
        Tcl_DStringAppend(ds_ptr, token->start, token->size);
    } else {
        fprintf(stderr, "TclAppendCommand_Token: other type of token: %d\n", token->type);
    }
    *out_i = i + 1;
    return TCL_OK;
}

int thtml_TclCompileCommand(Tcl_Interp *interp, Tcl_Obj *blocks_list_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr, const char *name, int nested) {
    DBG(fprintf(stderr, "thtml_TclCompileCommand\n"));

    Tcl_Token *token = &parse_ptr->tokenPtr[0];

    Tcl_DStringAppend(ds_ptr, "\n#CompileCommand: ", -1);
    Tcl_DStringAppend(ds_ptr, token->start, token->size);

    // if it is "expr" command, then call thtml_CCompileExpr
    if (token->type == TCL_TOKEN_SIMPLE_WORD) {
        if (token->size == 4 && 0 == strncmp(token->start, "expr", 4)) {

//            Tcl_Token *expr_token1 = &parse_ptr->tokenPtr[0];
//
//            fprintf(stderr, "numTokens: %ld\n", parse_ptr->numTokens);
//            fprintf(stderr, "expr_token->type: %d\n", expr_token1->type);
//            fprintf(stderr, "expr_token->size: %ld\n", expr_token1->size);
//            fprintf(stderr, "expr_token->start: %s\n", expr_token1->start);
//            fprintf(stderr, "expr_token->numComponents: %ld\n", expr_token1->numComponents);

            if (parse_ptr->numTokens == 4) {
                // handles the case when: expr { $x + $y }
                Tcl_Token *expr_token = &parse_ptr->tokenPtr[2];

                Tcl_Parse expr_parse;
                if (TCL_OK != Tcl_ParseExpr(interp, expr_token->start + 1, expr_token->size - 2, &expr_parse)) {
                    Tcl_FreeParse(&expr_parse);
                    return TCL_ERROR;
                }


                if (TCL_OK != thtml_TclCompileExpr(interp, blocks_list_ptr, ds_ptr, &expr_parse, name)) {
                    Tcl_FreeParse(&expr_parse);
                    return TCL_ERROR;
                }
                Tcl_FreeParse(&expr_parse);
                return TCL_OK;

            }

        }
    }

    Tcl_DStringAppend(ds_ptr, "\n#CompileCommand: numTokens=", -1);
    Tcl_DStringAppend(ds_ptr, Tcl_GetString(Tcl_NewIntObj(parse_ptr->numTokens)), -1);

    if (nested) {
        Tcl_DStringAppend(ds_ptr, "\nappend __ds_", -1);
        Tcl_DStringAppend(ds_ptr, name, -1);
        Tcl_DStringAppend(ds_ptr, "__ {[}", -1);
    }

    Tcl_Size i = 1;
    int first = 1;
    while (i < parse_ptr->numTokens) {
        if (!first) {
            Tcl_DStringAppend(ds_ptr, "\nappend __ds_", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "__ { }", -11);
        }

        if (TCL_OK != thtml_TclAppendCommand_Token(interp, blocks_list_ptr, ds_ptr, parse_ptr, i, &i, name, NULL, 1)) {
            fprintf(stderr, "thtml_TclAppendCommand_Token failed\n");
            return TCL_ERROR;
        }

        first = 0;
        // fprintf(stderr, "i: %d / %d\n", i, parse_ptr->numTokens);
    }

    if (nested) {
        Tcl_DStringAppend(ds_ptr, "\nappend __ds_", -1);
        Tcl_DStringAppend(ds_ptr, name, -1);
        Tcl_DStringAppend(ds_ptr, "__ {]}", -1);
    }


    // set __val3_subcmd1__ $__ds_val3_subcmd1__
    Tcl_DStringAppend(ds_ptr, "\nset __", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__ $__ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__", -1);

    // puts $__val3_subcmd1__
//    Tcl_DStringAppend(ds_ptr, "\nputs $__", -1);
//    Tcl_DStringAppend(ds_ptr, name, -1);
//    Tcl_DStringAppend(ds_ptr, "__", -1);

    return TCL_OK;
}
int
thtml_TclCompileForeachList(Tcl_Interp *interp, Tcl_Obj *blocks_list_ptr, Tcl_DString *ds_ptr, Tcl_Parse *parse_ptr, const char *name) {
    Tcl_DStringAppend(ds_ptr, "\nset __ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__ {}", -1);

//    char ds_name[64];
//    snprintf(ds_name, 64, "__ds_%s__", name);

    for (int i = 0; i < parse_ptr->numTokens; i++) {
        Tcl_Token *token = &parse_ptr->tokenPtr[i];
        if (token->type == TCL_TOKEN_TEXT) {
            Tcl_DStringAppend(ds_ptr, "\nappend __ds_", -1);
            Tcl_DStringAppend(ds_ptr, name, -1);
            Tcl_DStringAppend(ds_ptr, "__ {", -1);
            Tcl_DStringAppend(ds_ptr, token->start, token->size);
            Tcl_DStringAppend(ds_ptr, "}", -1);
        } else if (token->type == TCL_TOKEN_BS) {
            Tcl_DStringAppend(ds_ptr, token->start, token->size);
        } else if (token->type == TCL_TOKEN_COMMAND) {
            SetResult("error compiling foreach list: command substitution not supported");
            return TCL_ERROR;
        } else if (token->type == TCL_TOKEN_VARIABLE) {
            if (TCL_OK != thtml_TclAppendVariable(interp, blocks_list_ptr, ds_ptr, parse_ptr, i, name, NULL, 0)) {
                return TCL_ERROR;
            }
            i++;
        } else {
            fprintf(stderr, "type: %d\n", token->type);
            SetResult("error parsing quoted string: unsupported token type");
            return TCL_ERROR;
        }
    }
    Tcl_DStringAppend(ds_ptr, "\nset __", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__ $__ds_", -1);
    Tcl_DStringAppend(ds_ptr, name, -1);
    Tcl_DStringAppend(ds_ptr, "__", -1);

    return TCL_OK;
}

int thtml_TclCompileForeachListCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr, "TclCompileForeachListCmd\n"));

    CheckArgs(4, 4, 1, "codearrVar text name");

    Tcl_Size text_length;
    char *text = Tcl_GetStringFromObj(objv[2], &text_length);

    Tcl_Parse parse;
    if (TCL_OK != Tcl_ParseQuotedString(interp, text, text_length, &parse, 0, NULL)) {
        Tcl_FreeParse(&parse);
        return TCL_ERROR;
    }

    Tcl_Size name_length;
    char *name = Tcl_GetStringFromObj(objv[3], &name_length);

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

    if (TCL_OK != thtml_TclCompileForeachList(interp, blocks_list_ptr, &ds, &parse, name)) {
        Tcl_FreeParse(&parse);
        Tcl_DStringFree(&ds);
        return TCL_ERROR;
    }

    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}
