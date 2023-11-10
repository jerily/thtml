namespace eval ::thtml::compiler {}

# special characters that denote the start and end of html Vs code blocks:
# \x02 - start of text
# \x03 - end of text
proc ::thtml::compiler::tcl_compile_root {codearrVar root} {
    upvar $codearrVar codearr

    set compiled_template ""
    append compiled_template "\n" "set ds \"\"" "\n"
    append compiled_template [tcl_transform \x02[compile_helper codearr $root]\x03]
    append compiled_template "\n" "return \$ds" "\n"
    return $compiled_template
}

proc ::thtml::compiler::tcl_compile_statement_val {codearrVar node} {
    upvar $codearrVar codearr

    set chain_of_keys [$node @val]
    set script [$node asText]

    # substitute template variables in script
    # todo: compile_subst is a temporary hack here, we need to implement a proper compiler for the template language
    set compiled_script [compile_subst codearr $script 1]

    set compiled_statement ""
    append compiled_statement "\x03" "\n" "dict set __data__ {*}${chain_of_keys} \[::thtml::runtime::tcl::evaluate_script \"${compiled_script}\"\]" "\x02"
    return $compiled_statement
}

proc ::thtml::compiler::tcl_compile_statement_if {codearrVar node} {
    upvar $codearrVar codearr

    set conditional [$node @if]
    set compiled_conditional [tcl_compile_statement_if_expr codearr $conditional]

    set compiled_statement ""
    append compiled_statement "\x03" "\n" "if \{ $compiled_conditional \} \{ " "\x02"
    append compiled_statement [compile_children codearr $node]
    append compiled_statement "\x03" "\n" "\} " "\x02"
    return $compiled_statement
}

proc ::thtml::compiler::temp_tcl_compile_statement_if_expr {codearrVar text} {
    upvar $codearrVar codearr

    set len [string length $text]
    set escaped 0
    set compiled_if_expr ""
    # todo: compile_subst is a temporary hack here, we need to implement a proper compiler for the template language
    append compiled_if_expr [compile_subst codearr $text 1]
    return $compiled_if_expr
}

proc ::thtml::compiler::tcl_compile_statement_foreach {codearrVar node} {
    upvar $codearrVar codearr

    set foreach_varnames [$node @foreach]
    set foreach_list [$node @in]
    set compiled_foreach_list [compile_subst codearr $foreach_list 1]

    set compiled_statement ""
    append compiled_statement "\x03" "\n" "foreach \{${foreach_varnames}\} \"${compiled_foreach_list}\" \{ " "\x02"

    push_block codearr [list varnames $foreach_varnames]
    append compiled_statement [compile_children codearr $node]
    pop_block codearr

    append compiled_statement "\x03" "\n" "\} " "\x02"
    return $compiled_statement
}

proc ::thtml::compiler::tcl_compile_statement_include {codearrVar node} {
    upvar $codearrVar codearr

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

    dom parse $template doc
    set root [$doc documentElement]

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

    set proc_name ::thtml::cache::__include__$filepath_md5

    set compiled_include "\x03"

    set argnames [list]
    foreach attname [$node attributes] {
        if { $attname eq {include} } { continue }
        lappend argnames $attname
    }

    set argvalues [list]
    foreach attname [$node attributes] {
        if { $attname eq {include} } { continue }
        lappend argvalues [compile_subst codearr [$node @$attname] 1]
    }

    push_block codearr [list varnames $argnames stop 1 include [list filepath $filepath_from_rootdir filepath_md5 $filepath_md5]]

    append compiled_include "\n" "\# " $filepath_from_rootdir
    append compiled_include "\n" "proc ${proc_name} {${argnames}} \{"
    append compiled_include "\n" "set __data__ \{\}" "\n"
    append compiled_include "\n" "set ds \"\"" "\n"
    append compiled_include [tcl_transform \x02[compile_helper codearr $root]\x03]
    append compiled_include "\n" "return \$ds"
    append compiled_include "\n" "\}"
    #puts argvalues=$argvalues
    append compiled_include "\n" "append ds \[eval ${proc_name} \"${argvalues}\"\]" "\n"
    append compiled_include "\x02"

    pop_block codearr

    return $compiled_include
}

proc ::thtml::compiler::tcl_compile_subst_var_from_simple {codearrVar varname inside_code_block} {
    upvar $codearrVar codearr
    if { $inside_code_block } {
        return "\$\{${varname}\}"
    } else {
        set compiled_subst_var ""
        append compiled_subst_var "\x03"
        append compiled_subst_var "\n" "append ds \$\{${varname}\}" "\n"
        append compiled_subst_var "\x02"
        return $compiled_subst_var
    }
}

proc ::thtml::compiler::tcl_compile_subst_var_from_dict {codearrVar varname parts inside_code_block} {
    upvar $codearrVar codearr
    if { $inside_code_block } {
        return "\[dict get \$\{${varname}\} {*}${parts}\]"
    } else {
        set compiled_subst_var ""
        append compiled_subst_var "\x03"
        append compiled_subst_var "\n" "append ds \[dict get \$\{${varname}\} {*}${parts}\]" "\n"
        append compiled_subst_var "\x02"
        return $compiled_subst_var
    }
}
