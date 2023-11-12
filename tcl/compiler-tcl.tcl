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
    set compiled_script [tcl_compile_script codearr $script]

    set compiled_statement ""
    append compiled_statement "\x03" "\n" "dict set __data__ {*}${chain_of_keys} \[::thtml::runtime::tcl::evaluate_script \"${compiled_script}\"\]" "\x02"
    return $compiled_statement
}

proc ::thtml::compiler::tcl_compile_statement_if {codearrVar node} {
    upvar $codearrVar codearr

    set conditional [$node @if]
    #puts conditional=$conditional
    set compiled_conditional [tcl_compile_expr codearr $conditional]

    set compiled_statement ""
    append compiled_statement "\x03" "\n" "if \{ $compiled_conditional \} \{ " "\x02"
    append compiled_statement [compile_children codearr $node]
    append compiled_statement "\x03" "\n" "\} " "\x02"
    return $compiled_statement
}

proc ::thtml::compiler::tcl_compile_statement_foreach {codearrVar node} {
    upvar $codearrVar codearr

    set foreach_varnames [$node @foreach]
    set foreach_list [$node @in]
    set compiled_foreach_list [tcl_compile_quoted_string codearr \"$foreach_list\"]

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
        lappend argvalues [tcl_compile_quoted_string codearr \"[$node @$attname]\"]
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
