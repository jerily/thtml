# Copyright Jerily LTD. All Rights Reserved.
# SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
# SPDX-License-Identifier: MIT.

namespace eval ::thtml::compiler {}

# special characters that denote the start and end of html Vs code blocks:
# \x02 - start of text
# \x03 - end of text
proc ::thtml::compiler::c_compile_root {codearrVar root} {
    upvar $codearrVar codearr

    set compiled_template ""
    append compiled_template "\n" "Tcl_DString __ds__;" "\n"
    append compiled_template "\n" "Tcl_DString *__ds_ptr__ = &__ds__;" "\n"
    append compiled_template "\n" "Tcl_DStringInit(__ds_ptr__);" "\n"
    foreach child [$root childNodes] {
        append compiled_template [c_transform \x02[compile_helper codearr $child]\x03]
    }
    append compiled_template "\n" "Tcl_SetObjResult(interp, Tcl_NewStringObj(Tcl_DStringValue(__ds_ptr__), Tcl_DStringLength(__ds_ptr__)));" "\n"
    append compiled_template "\n" "return TCL_OK;" "\n"
    return $compiled_template
}

proc ::thtml::compiler::c_compile_statement_val {codearrVar node} {
    upvar $codearrVar codearr

    set chain_of_keys [$node @val]
    set script [$node asText]

    # substitute template variables in script
    set compiled_script [c_compile_script codearr $script]

    set compiled_statement ""
    append compiled_statement "\x03" "\n" "dict set __data__ {*}${chain_of_keys} \[::thtml::runtime::tcl::evaluate_script \{${compiled_script}\}\]" "\x02"
    return $compiled_statement
}

proc ::thtml::compiler::c_compile_statement_if {codearrVar node} {
    upvar $codearrVar codearr

    set conditional_num [incr codearr(if_count)]
    set conditional [$node @if]
    #puts conditional=$conditional
    set compiled_conditional [c_compile_expr codearr $conditional "flag${conditional_num}"]

    set compiled_statement ""
    append compiled_statement "\x03" "\n" $compiled_conditional "\x02"
    append compiled_statement "\x03" "\n" "if ( __flag${conditional_num}__ ) \{ " "\x02"
    append compiled_statement [compile_children codearr $node]
    append compiled_statement "\x03" "\n" "\} " "\x02"
    return $compiled_statement
}

proc ::thtml::compiler::c_compile_statement_foreach {codearrVar node} {
    upvar $codearrVar codearr

    set foreach_num [incr codearr(foreach_count)]
    set foreach_varnames [$node @foreach]
    set foreach_indexvar [$node @indexvar ""]
    set foreach_list [$node @in]

    set compiled_statement ""
    append compiled_statement "\x03" "\{"

    set varnames $foreach_varnames
    if { $foreach_indexvar ne "" } {
        append compiled_statement "\n" "Tcl_Size __indexvar_${foreach_indexvar}__ = 0;"
        append compiled_statement "\n" "Tcl_Obj *${foreach_indexvar} = Tcl_NewIntObj(__indexvar_${foreach_indexvar}__);"
        append compiled_statement "\n" "Tcl_IncrRefCount(${foreach_indexvar});" "\n"
        lappend varnames $foreach_indexvar
    }

    set compiled_foreach_list [c_compile_foreach_list codearr \"$foreach_list\" "list${foreach_num}"]

    append compiled_statement "\n" ${compiled_foreach_list}
    append compiled_statement "\n" "Tcl_IncrRefCount(__list${foreach_num}__);"
    append compiled_statement "\n" "Tcl_Obj *__list${foreach_num}_len__;"
    append compiled_statement "\n" "if (TCL_OK != Tcl_ListObjLength(interp, __list${foreach_num}__, &__list${foreach_num}_len__)) {return TCL_ERROR;}"
    append compiled_statement "\n" "for (int __i${foreach_num}__ = 0; __i${foreach_num}__ < __list${foreach_num}_len__; __i${foreach_num}__++)  \{"
    append compiled_statement "\n" "Tcl_Obj *__elem${foreach_num}__;"
    append compiled_statement "\n" "if (TCL_OK != Tcl_ListObjIndex(interp, __list${foreach_num}__, __i${foreach_num}__, &__elem${foreach_num}__)) {return TCL_ERROR;}"
    set foreach_varname_i 0
    foreach foreach_varname $foreach_varnames {
        append compiled_statement "\n" "Tcl_Obj *${foreach_varname};"
        append compiled_statement "\n" "if (TCL_OK != Tcl_ListObjIndex(interp, __elem${foreach_num}__, ${foreach_varname_i}, &${foreach_varname})) {return TCL_ERROR;}"
        incr foreach_varname_i
    }


    append compiled_statement "\x02"

    push_block codearr [list varnames $varnames]
    append compiled_statement [compile_children codearr $node]
    pop_block codearr

    append compiled_statement "\x03"
    if { $foreach_indexvar ne "" } {
        append compiled_statement "\n" "__indexvar_${foreach_indexvar}__++;" "\n"
        append compiled_statement "\n" "Tcl_SetIntObj(${foreach_indexvar}, __indexvar_${foreach_indexvar}__);"
    }
    append compiled_statement "\n" "\} "
    append compiled_statement "\n" "Tcl_DecrRefCount(__list${foreach_num}__);" "\n"
    append compiled_statement "\n" "Tcl_DecrRefCount(${foreach_indexvar});" "\n"
    append compiled_statement "\n" "\}" "\x02"
    return $compiled_statement
}

proc ::thtml::compiler::c_compile_statement_include {codearrVar node} {
    upvar $codearrVar codearr

    set include_num [incr codearr(include_count)]

    set filepath [::thtml::util::resolve_filepath [$node @include]]
    set filepath_from_rootdir [string range $filepath [string length [::thtml::get_rootdir]] end]
    set filepath_md5 [::thtml::util::md5 $filepath_from_rootdir]

    # check that we do not have circular dependencies
    foreach block $codearr(blocks) {
        if { [dict exists $block include] && [dict get $block include filepath_md5] eq $filepath_md5 } {
            error "circular dependency detected"
        }
    }

    set fp [open $filepath]
    set template [read $fp]
    close $fp

    set escaped_template [string map {{&&} {&amp;&amp;}} $template]
    dom parse -paramentityparsing never -- <root>$escaped_template</root> doc
    set root [$doc documentElement]
    ::thtml::rewrite $root

    # replace the slave node with the children of the include node
    set slave [lindex [$root getElementsByTagName "slave"] 0]
    if { $slave ne {} } {
        set pn [$slave parentNode]
        foreach child [$node childNodes] {
            $pn insertBefore $child $slave
        }
        $slave delete
    }

    # compile the include template into a procedure and call it

    set proc_name __include__$filepath_md5

    set compiled_include "\x03"

    set varnames [list]
    set argnames [list]
    lappend argnames "Tcl_Obj *__data__"
    foreach attname [$node attributes] {
        if { $attname eq {include} } { continue }
        lappend varnames $attname
        lappend argnames "Tcl_Obj *$attname"
    }

    set argnum 1
    set argvalues [list]
    lappend argvalues "__data__"
    foreach attname [$node attributes] {
        if { $attname eq {include} } { continue }
        append compiled_include "\n" [c_compile_quoted_arg codearr \"[$node @$attname]\" "include${include_num}_arg${argnum}_${attname}"]
        lappend argvalues "__include${include_num}_arg${argnum}_${attname}__"
        incr argnum
    }


    push_block codearr [list varnames $varnames stop 1 include [list filepath $filepath_from_rootdir filepath_md5 $filepath_md5]]

    append compiled_include "\n" "// " $filepath_from_rootdir
    append compiled_include "\n" "int ${proc_name} (Tcl_Interp *__interp__, Tcl_DString *__ds_ptr__, [join ${argnames} {, }]) \{"
    foreach child [$root childNodes] {
        append compiled_include [c_transform \x02[compile_helper codearr $child]\x03]
    }
    append compiled_include "\n" "return TCL_OK;"
    append compiled_include "\n" "\}"
    #puts argvalues=$argvalues
    append compiled_include "\n" "if (TCL_OK != ${proc_name}(__interp__, __ds__ptr__, [join ${argvalues} {, }])) {return TCL_ERROR;}" "\n"
    append compiled_include "\x02"

    pop_block codearr

    return $compiled_include
}
