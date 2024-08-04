# Copyright Jerily LTD. All Rights Reserved.
# SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
# SPDX-License-Identifier: MIT.

namespace eval ::thtml::compiler {}

# special characters that denote the start and end of html Vs code blocks:
# \x02 - start of text
# \x03 - end of text
proc ::thtml::compiler::tcl_compile_root {codearrVar root} {
    upvar $codearrVar codearr

    set compiled_template ""
    append compiled_template "\n" "set __ds_default__ \"\"" "\n"
    foreach child [$root childNodes] {
        append compiled_template [tcl_transform \x02[compile_helper codearr $child]\x03]
    }
    append compiled_template "\n" "set __ds_default__" "\n"
    return $compiled_template
}

proc ::thtml::compiler::tcl_compile_statement_bundle {codearrVar node} {
    upvar $codearrVar codearr

    set top_component [::thtml::compiler::top_component codearr]
    set md5 [dict get $top_component md5]

    set suffix $md5
    lappend codearr(bundle_metadata) suffix $suffix

    set filename "bundle_${suffix}.js"

    append compiled_script "<script src=\\\""
    append compiled_script "\x03" [tcl_compile_quoted_string codearr "\"[$node @url_prefix]\""] "\x02"
    append compiled_script "${filename}\\\"></script>"
    return $compiled_script
}

proc ::thtml::compiler::tcl_compile_statement_js {codearrVar node} {
    upvar $codearrVar codearr

    set js_num [incr codearr(js_count)]

    set top_component [::thtml::compiler::top_component codearr]
    set md5 [dict get $top_component md5]

    set js [$node asText]
    set js_args ""
    set js_vals ""
    if [$node hasAttribute "args"] {
        set first 1
        foreach {name value} [$node @args] {
            if { $first } {
                set first 0
            } else {
                append js_args ", "
                append js_vals ", "
            }
            append js_args $name
            append js_vals $value
        }
    }

    lappend codearr(js_code,$md5) $js_num $js_args $js
    lappend codearr(bundle_js_names) $md5

    append compiled_script "<script>THTML.js_${md5}.js_${md5}_${js_num}("
    append compiled_script "\x03" [tcl_compile_quoted_string codearr "\"$js_vals\""] "\x02"
    append compiled_script ");</script>"
    return $compiled_script
}

proc ::thtml::compiler::tcl_compile_statement_val {codearrVar node} {
    upvar $codearrVar codearr

    set val_num [incr codearr(val_count)]

    set chain_of_keys [$node @val]
    set script [$node asText]

    # substitute template variables in script
    set compiled_script [tcl_compile_script codearr $script "val${val_num}"]

    set compiled_statement ""
    append compiled_statement "\x03"
    append compiled_statement $compiled_script
    # using evaluate_script so that a return statement in a val command
    # will return from the procedure, not from the whole script
    append compiled_statement "\n" "dict set __data__ {*}${chain_of_keys} \[::thtml::runtime::tcl::evaluate_script \$__ds_val${val_num}__\]" "\x02"
    return $compiled_statement
}

proc ::thtml::compiler::tcl_compile_statement_if {codearrVar node} {
    upvar $codearrVar codearr

    set conditional_num [incr codearr(if_count)]

    set conditional [$node @if]
    #puts conditional=$conditional
    set compiled_conditional [tcl_compile_expr codearr $conditional "flag${conditional_num}"]

    set compiled_statement ""
    append compiled_statement "\x03" "\n" $compiled_conditional "\x02"
    append compiled_statement "\x03" "\n" "if \{ \$__flag${conditional_num}__ \} \{ " "\x02"
    append compiled_statement [compile_children codearr $node]
    append compiled_statement "\x03" "\n" "\} " "\x02"
    return $compiled_statement
}

proc ::thtml::compiler::tcl_compile_statement_foreach {codearrVar node} {
    upvar $codearrVar codearr


    set foreach_num [incr codearr(foreach_count)]
    set foreach_varnames [$node @foreach]
    set foreach_indexvar [$node @indexvar ""]
    set foreach_list [$node @in]

    set compiled_statement ""
    append compiled_statement "\x03"

    set varnames $foreach_varnames
    if { $foreach_indexvar ne "" } {
        append compiled_statement "\n" "set __indexvar_${foreach_indexvar}__ 0"
        append compiled_statement "\n" "set ${foreach_indexvar} \$__indexvar_${foreach_indexvar}__"
        lappend varnames $foreach_indexvar
    }

    set compiled_foreach_list [tcl_compile_foreach_list codearr \"$foreach_list\" "list${foreach_num}"]

    append compiled_statement "\n" ${compiled_foreach_list}
#    append compiled_statement "\n" "puts list${foreach_num}=\$__list${foreach_num}__"
    append compiled_statement "\n" "set __list${foreach_num}_len__ \[llength \$__list${foreach_num}__\]"
    append compiled_statement "\n" "for \{ set __i${foreach_num}__ 0 \} \{ \$__i${foreach_num}__ < \$__list${foreach_num}_len__ \} \{ incr __i${foreach_num}__ [llength $foreach_varnames] \}  \{"
#    append compiled_statement "\n" "set __elem${foreach_num}__ \[lindex \$__list${foreach_num}__ \$__i${foreach_num}__\]"
    set foreach_varname_i 0
    foreach foreach_varname $foreach_varnames {
        append compiled_statement "\n" "set ${foreach_varname} \[lindex \$__list${foreach_num}__ \[expr \{ ${foreach_varname_i} + \$__i${foreach_num}__ \}\]\]"
#        append compiled_statement "\n" "puts ${foreach_varname}=\$${foreach_varname}"
        incr foreach_varname_i
    }


    append compiled_statement "\x02"

    push_block codearr [list varnames $varnames]
    append compiled_statement [compile_children codearr $node]
    pop_block codearr

    append compiled_statement "\x03"
    if { $foreach_indexvar ne "" } {
        append compiled_statement "\n" "incr __indexvar_${foreach_indexvar}__" "\n"
        append compiled_statement "\n" "set ${foreach_indexvar} \$__indexvar_${foreach_indexvar}__"
    }
    append compiled_statement "\n" "\} "
    append compiled_statement "\n" "\x02"
    return $compiled_statement
}

proc ::thtml::compiler::tcl_compile_statement_include {codearrVar node} {
    upvar $codearrVar codearr

    set include_num [incr codearr(include_count)]

    set filepath [::thtml::util::resolve_filepath [$node @include]]
    set filepath_from_rootdir [string range $filepath [string length [::thtml::get_rootdir]] end]
    set filepath_md5 [::thtml::util::md5 $filepath_from_rootdir]

    push_component codearr [list md5 $filepath_md5]

    set tcl_code ""
    set tcl_filepath "[file rootname $filepath].tcl"
    if { [file exists $tcl_filepath] } {
        set fp [open $tcl_filepath]
        set tcl_code [read $fp]
        close $fp
    }

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

    ::thtml::process_node_module_imports codearr $root
    ::thtml::rewrite_template_imports $root

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

    set proc_name ::thtml::cache::__include_${filepath_md5}_${slave_md5}__

    set tcl_proc_name ""
    if { $tcl_code ne {} } {
        set tcl_proc_name ::thtml::cache::__include_${filepath_md5}__
    }

    set compiled_include "\x03"

#    set varnames [list]
    set argnames [list]
    foreach attname [$node attributes] {
        if { $attname eq {include} } { continue }
        lappend argnames $attname
#        lappend varnames $attname
    }

    append compiled_include "\n" "\# " $filepath_from_rootdir

    set arg_num 0
    set argvalues [list]
    foreach attname [$node attributes] {
        if { $attname eq {include} } { continue }
        append compiled_include "\n" [tcl_compile_quoted_string codearr \"[$node @$attname]\" include${include_num}_arg${arg_num}]
        lappend argvalues "\$__ds_include${include_num}_arg${arg_num}__"
        incr arg_num
    }

    #push_block codearr [list varnames $varnames stop 1 include [list filepath $filepath_from_rootdir filepath_md5 $filepath_md5]]
    push_block codearr [list varnames {} stop 1 include [list filepath $filepath_from_rootdir filepath_md5 $filepath_md5]]

    set seen [get_seen codearr $proc_name]
    if { !$seen } {

        if { $tcl_code ne {} } {
            append compiled_include_proc "\n" "proc ${tcl_proc_name} {__data__} \{"
            append compiled_include_proc "\n" $tcl_code
            append compiled_include_proc "\n" "\}"
        }

        append compiled_include_proc "\n" "proc ${proc_name} {__data__} \{"
        append compiled_include_proc "\n" "set __ds_default__ \"\"" "\n"
        foreach child [$root childNodes] {
            append compiled_include_proc [tcl_transform \x02[compile_helper codearr $child]\x03]
        }
        append compiled_include_proc "\n" "return \$__ds_default__"
        append compiled_include_proc "\n" "\}"
        append codearr(defs) $compiled_include_proc
    }

    #puts argnames=$argnames,argvalueVars=$argvalueVars

    append compiled_include "\n" "set __list_include${include_num}__ \[list\]"
    foreach argname $argnames argvalue $argvalues {
        append compiled_include "\n" "lappend __list_include${include_num}__ $argname $argvalue"
    }
    set argdata_code "\$__data__"
    if { $tcl_code ne {} } {
        set argdata_code "\[${tcl_proc_name} \$__data__\]"
    }
    append compiled_include "\n" "set __data_include${include_num}__ \[dict merge $argdata_code \$__list_include${include_num}__\]"
    #append compiled_include "\n" "puts \$__data_include${include_num}__"
    append compiled_include "\n" "append __ds_default__ \[${proc_name} \$__data_include${include_num}__\]" "\n"
    append compiled_include "\x02"

    pop_block codearr
    pop_component codearr

    return $compiled_include
}
