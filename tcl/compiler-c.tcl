# Copyright Jerily LTD. All Rights Reserved.
# SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
# SPDX-License-Identifier: MIT.

namespace eval ::thtml::compiler {}

# special characters that denote the start and end of html Vs code blocks:
# \x02 - start of text
# \x03 - end of text
proc ::thtml::compiler::c_compile_root {codearrVar root} {
    upvar $codearrVar codearr

    append compiled_template "\n" "Tcl_Obj *__data__ = Tcl_DuplicateObj(objv\[1\]);"
    append compiled_template "\n" "Tcl_DString __ds_default_base__;" "\n"
    append compiled_template "\n" "Tcl_DString *__ds_default__ = &__ds_default_base__;" "\n"
    append compiled_template "\n" "Tcl_DStringInit(__ds_default__);" "\n"
    foreach child [$root childNodes] {
        append compiled_template [c_transform \x02[compile_helper codearr $child]\x03]
    }
    append compiled_template "\n" "Tcl_SetObjResult(__interp__, Tcl_NewStringObj(Tcl_DStringValue(__ds_default__), Tcl_DStringLength(__ds_default__)));" "\n"
    append compiled_template "\n" "return TCL_OK;" "\n"
    return $compiled_template
}

proc ::thtml::compiler::c_compile_statement_val {codearrVar node} {
    upvar $codearrVar codearr

    set val_num [incr codearr(val_count)]

    set chain_of_keys [$node @val]
    set script [$node asText]

    # substitute template variables in script
    set compiled_script [c_compile_script codearr $script "val${val_num}"]

    set compiled_statement ""
    append compiled_statement "\x03"
    append compiled_statement "\n" ${compiled_script}

    set key_objs {}
    for {set i 0} {$i < [llength $chain_of_keys]} {incr i} {
        set key [lindex $chain_of_keys $i]
        append compiled_statement "\n" "Tcl_Obj *__val${val_num}_key_${key}__ = Tcl_NewStringObj(\"${key}\", -1);"
        append compiled_statement "\n" "Tcl_IncrRefCount(__val${val_num}_key_${key}__);"
        lappend key_objs "__val${val_num}_key_${key}__"
    }
    append compiled_statement "\n" "Tcl_Obj *__val${val_num}_keyv__\[\] = \{ [join $key_objs {,}] \};"

    # todo: decr ref count when it errs
    append compiled_statement "\n" "if (TCL_OK != Tcl_DictObjPutKeyList(__interp__, __data__, [llength $chain_of_keys], __val${val_num}_keyv__, Tcl_GetObjResult(__interp__))) {return TCL_ERROR;}"

    for {set i 0} {$i < [llength $chain_of_keys]} {incr i} {
        set key [lindex $chain_of_keys $i]
        append compiled_statement "\n" "Tcl_DecrRefCount(__val${val_num}_key_${key}__);"
    }

#    append compiled_statement "\n" "fprintf(stderr, \"val${val_num} = %s\\n\", Tcl_GetString(Tcl_GetObjResult(__interp__)));"
    append compiled_statement "\n" "Tcl_ResetResult(__interp__);"

    append compiled_statement "\x02"
    return $compiled_statement
}

proc ::thtml::compiler::c_compile_statement_if {codearrVar node} {
    upvar $codearrVar codearr

    set conditional_num [incr codearr(if_count)]

    set conditional [$node @if]
    #puts conditional=$conditional
    set compiled_conditional [c_compile_expr codearr $conditional "flag${conditional_num}"]

    append compiled_conditional "\n" "int __flag${conditional_num}_bool__;"
    append compiled_conditional "\n" "if (Tcl_GetBooleanFromObj(__interp__, __flag${conditional_num}__, &__flag${conditional_num}_bool__) != TCL_OK) {return TCL_ERROR;}"
    append compiled_conditional "\n" "Tcl_DecrRefCount(__flag${conditional_num}__);"

    set compiled_statement ""
    append compiled_statement "\x03" "\n" $compiled_conditional "\x02"
    append compiled_statement "\x03" "\n" "if ( __flag${conditional_num}_bool__ ) \{ " "\x02"
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
#    append compiled_statement "\n" "fprintf(stderr, \"list${foreach_num} = %s\\n\", Tcl_GetString(__list${foreach_num}__));"
    append compiled_statement "\n" "Tcl_Size __list${foreach_num}_len__;"
    append compiled_statement "\n" "if (TCL_OK != Tcl_ListObjLength(__interp__, __list${foreach_num}__, &__list${foreach_num}_len__)) {return TCL_ERROR;}"
#    append compiled_statement "\n" "fprintf(stderr, \"list${foreach_num}_len = %d\\n\", __list${foreach_num}_len__);"
    append compiled_statement "\n" "for (int __i${foreach_num}__ = 0; __i${foreach_num}__ < __list${foreach_num}_len__; __i${foreach_num}__ += [llength $foreach_varnames])  \{"
    set foreach_varname_i 0
    foreach foreach_varname $foreach_varnames {
        append compiled_statement "\n" "Tcl_Obj *${foreach_varname};"
        append compiled_statement "\n" "if (TCL_OK != Tcl_ListObjIndex(__interp__, __list${foreach_num}__, ${foreach_varname_i} + __i${foreach_num}__, &${foreach_varname})) {return TCL_ERROR;}"
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
    if { $foreach_indexvar ne "" } {
        append compiled_statement "\n" "Tcl_DecrRefCount(${foreach_indexvar});" "\n"
    }
    append compiled_statement "\n" "\}" "\x02"
    return $compiled_statement
}

proc ::thtml::compiler::c_compile_statement_include {codearrVar node} {
    upvar $codearrVar codearr

    set include_num [incr codearr(include_count)]

    set filepath [::thtml::util::resolve_filepath [$node @include]]
    puts filepath=$filepath,rootdir=[::thtml::get_rootdir]
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

    set escaped_template [::thtml::escape_template $template]
    dom parse -ignorexmlns -paramentityparsing never -- <root>$escaped_template</root> doc
    set root [$doc documentElement]
    ::thtml::rewrite $root

    # replace the slave node with the children of the include node
    set slave_md5 "noslave"
    set slave [lindex [$root getElementsByTagName "slave"] 0]
    if { $slave ne {} } {
        set slave_md5 [::thtml::util::md5 [$node asXML]]
        set pn [$slave parentNode]
        foreach child [$node childNodes] {
            $pn insertBefore $child $slave
        }
        $slave delete
    }

    # compile the include template into a procedure and call it

    set proc_name __include_${filepath_md5}_${slave_md5}__

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
        append compiled_include "\n" "Tcl_IncrRefCount(__include${include_num}_arg${argnum}_${attname}__);"
        lappend argvalues "__include${include_num}_arg${argnum}_${attname}__"
        incr argnum
    }


    push_block codearr [list varnames $varnames stop 1 include [list filepath $filepath_from_rootdir filepath_md5 $filepath_md5]]

    set seen [get_seen codearr $proc_name]
    if { !$seen } {
        append compiled_include_func "\n" "// " $filepath_from_rootdir
        append compiled_include_func "\n" "int ${proc_name} (Tcl_Interp *__interp__, Tcl_DString *__ds_default__, [join ${argnames} {, }]) \{"
        foreach child [$root childNodes] {
            append compiled_include_func [c_transform \x02[compile_helper codearr $child]\x03]
        }
        append compiled_include_func "\n" "return TCL_OK;"
        append compiled_include_func "\n" "\}"
        append codearr(defs) $compiled_include_func
    }
    set_seen codearr $proc_name

    #puts argvalues=$argvalues
    append compiled_include "\n" "if (TCL_OK != ${proc_name}(__interp__, __ds_default__, [join ${argvalues} {, }])) {return TCL_ERROR;}" "\n"
    set argnum 1
    foreach attname [$node attributes] {
        if { $attname eq {include} } { continue }
        append compiled_include "\n" "Tcl_DecrRefCount(__include${include_num}_arg${argnum}_${attname}__);"
        incr argnum
    }
    append compiled_include "\x02"

    pop_block codearr

    return $compiled_include
}
