# Copyright Jerily LTD. All Rights Reserved.
# SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
# SPDX-License-Identifier: MIT.

package require tdom

namespace eval ::thtml {
    variable EMPTY_ELEMENTS_IN_HTML {
        area base basefont br col frame
        hr img input isindex link meta param
    }
    variable rootdir
}

namespace eval ::thtml::cache {}

proc ::thtml::render {template __data__} {
    set compiled_template [compile $template]
    return "<!doctype html>[eval $compiled_template]"
}

# special characters that denote the start and end of html Vs code blocks:
# \x02 - start of text
# \x03 - end of text
proc ::thtml::compile {template} {
    dom parse $template doc
    set root [$doc documentElement]

    array set codearr [list blocks {}]
    set compiled_template ""
    append compiled_template "\n" "set ds \"\"" "\n"
    append compiled_template [transform \x02[compile_helper codearr $root]\x03]
    append compiled_template "\n" "return \$ds" "\n"
    return $compiled_template
}

proc ::thtml::compile_helper {codearrVar node} {
    upvar $codearrVar codearr

    set node_type [$node nodeType]
    if { $node_type eq {TEXT_NODE} } {
        return [compile_subst codearr [$node nodeValue]]
    } elseif { $node_type eq {ELEMENT_NODE} } {
        set tag [$node tagName]
        if { $tag eq {tpl} } {
            return [compile_statement codearr $node]
        } else {
            return [compile_element codearr $node]
        }
    }
}

proc ::thtml::compile_element {codearrVar node} {
    upvar $codearrVar codearr
    variable EMPTY_ELEMENTS_IN_HTML

    set tag [$node tagName]
    set compiled_element "<${tag}"
    foreach attname [$node attributes] {
        set attvalue [$node @$attname]
        set compiled_attvalue [compile_subst codearr $attvalue]
        append compiled_element " ${attname}=\"${compiled_attvalue}\""
    }

    if { $tag in $EMPTY_ELEMENTS_IN_HTML } {
        append compiled_element "/>"
        set ctag ""
    } else {
        append compiled_element ">"
        set ctag "</${tag}>"
    }

    append compiled_element [compile_children codearr $node]
    append compiled_element $ctag
    return $compiled_element
}

proc ::thtml::compile_statement {codearrVar node} {
    upvar $codearrVar codearr

    if { [$node hasAttribute "if"] } {
        return [compile_statement_if codearr $node]
    } elseif { [$node hasAttribute "foreach"] } {
        return [compile_statement_foreach codearr $node]
    } elseif { [$node hasAttribute "include"] } {
        return [compile_statement_include codearr $node]
    } elseif { [$node hasAttribute "eval"] } {
        return [compile_statement_eval codearr $node]
    } else {
        return [compile_children codearr $node]
    }
}

proc ::thtml::compile_statement_if {codearrVar node} {
    upvar $codearrVar codearr

    set conditional [$node @if]
    set compiled_conditional [compile_statement_if_expr codearr $conditional]

    set compiled_statement ""
    append compiled_statement "\x03" "\n" "if \{ $compiled_conditional \} \{ " "\x02"
    append compiled_statement [compile_children codearr $node]
    append compiled_statement "\x03" "\n" "\} " "\x02"
    return $compiled_statement
}

proc ::thtml::compile_statement_if_expr {codearrVar text} {
    upvar $codearrVar codearr

    set len [string length $text]
    set escaped 0
    set compiled_if_expr ""
    # todo: implementation of valid_if_expr in C
    #if { ![valid_if_expr $text] } {
    #    error "invalid if expression"
    #}
    append compiled_if_expr [compile_subst codearr $text]
    return $compiled_if_expr
}

proc ::thtml::compile_statement_foreach {codearrVar node} {
    upvar $codearrVar codearr

    set foreach_varnames [$node @foreach]
    set foreach_list [$node @in]
    set compiled_foreach_list [compile_subst codearr $foreach_list]

    set compiled_statement ""
    append compiled_statement "\x03" "\n" "foreach \{${foreach_varnames}\} \"${compiled_foreach_list}\" \{ " "\x02"

    ::thtml::push_block codearr [list varnames $foreach_varnames]
    append compiled_statement [compile_children codearr $node]
    ::thtml::pop_block codearr

    append compiled_statement "\x03" "\n" "\} " "\x02"
    return $compiled_statement
}

proc ::thtml::compile_statement_include {codearrVar node} {
    upvar $codearrVar codearr

    set filepath [resolve_filepath [$node @include]]
    set filepath_from_rootdir [string range $filepath [string length [get_rootdir]] end]
    set filepath_md5 [md5 $filepath_from_rootdir]

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
        lappend argvalues [compile_subst codearr [$node @$attname]]
    }

    push_block codearr [list varnames $argnames stop 1 include [list filepath $filepath_from_rootdir filepath_md5 $filepath_md5]]

    append compiled_include "\n" "\# " $filepath_from_rootdir
    append compiled_include "\n" "proc ${proc_name} {${argnames}} \{"
    append compiled_include "\n" "set __data__ \{\}" "\n"
    append compiled_include "\n" "set ds \"\"" "\n"
    append compiled_include [transform \x02[compile_helper codearr $root]\x03]
    append compiled_include "\n" "return \$ds"
    append compiled_include "\n" "\}"
    append compiled_include "\n" "append ds \[eval ${proc_name} \"${argvalues}\"\]" "\n"
    append compiled_include "\x02"

    pop_block codearr

    return $compiled_include
}

proc ::thtml::compile_children {codearrVar node} {
    upvar $codearrVar codearr

    set compiled_children ""
    foreach child [$node childNodes] {
        append compiled_children [compile_helper codearr $child]
    }
    return $compiled_children
}

proc ::thtml::compile_subst {codearrVar text} {
    upvar $codearrVar codearr

    set len [string length $text]
    set escaped 0
    set compiled_subst ""
    for {set i 0} {$i < $len} {incr i} {
        set ch [string index $text $i]
        if { $ch eq "\\" } {
            if { $escaped } {
                append compiled_subst $ch
                set escaped 0
            } else {
                set escaped 1
            }
        } elseif { $ch eq "\$" || $ch eq "\[" || $ch eq "\]" || $ch eq "\"" || $ch eq "\{" || $ch eq "\}" } {
            if { $escaped } {
                error "invalid escape sequence in substitution"
            }
            append compiled_subst "\\${ch}"
        } elseif { $ch eq "@" && $i + 1 < $len } {
            set next_ch [string index $text [expr {$i + 1}]]
            if { $next_ch eq "\{" } {
                set count 1
                for {set j $i} {$j < $len} {incr j} {
                    set ch [string index $text $j]
                    if { $ch eq "\}" } {
                        incr count -1
                        if { $count == 0 } {
                            break
                        }
                    }
                }
                if { $count != 0 } {
                    error "unmatched braces in substitution"
                }
                set varname [string range $text [expr {$i + 2}] [expr {$j - 1}]]
                append compiled_subst [compile_subst_var codearr $varname]
                set i $j
            }
        } else {
            if { $escaped } {
                append compiled_subst $ch
                set escaped 0
            } else {
                append compiled_subst $ch
            }
        }
    }
    return $compiled_subst
}

proc ::thtml::compile_subst_var {codearrVar varname} {
    upvar $codearrVar codearr

    set parts [split $varname "."]
    set parts_length [llength $parts]
    foreach block $codearr(blocks) {
        foreach block_varname [dict get $block varnames] {
            if { ${block_varname} eq ${varname} } {
                if { $parts_length == 1 } {
                    return "\$\{${varname}\}"
                } else {
                    return "\[dict get \"\$\{${varname}\}\" {*}${parts}\]"
                }
            }
        }
        if { [dict exists $block stop] } {
            # variable not found in blocks
            # break out of the loop and try
            # the __data__ dictionary of the
            # current proc
            break
        }
    }
    return "\[dict get \$\{__data__\} {*}${parts}\]"
}

### codearr manipulation

proc ::thtml::push_block {codearrVar block} {
    upvar $codearrVar codearr
    set codearr(blocks) [linsert $codearr(blocks) 0 $block]
}

proc ::thtml::pop_block {codearrVar} {
    upvar $codearrVar codearr
    set codearr(blocks) [lrange $codearr(blocks) 1 end]
}

proc ::thtml::top_block {codearrVar} {
    upvar $codearrVar codearr
    return [lindex $codearr(blocks) 0]
}

### helper procs

proc ::thtml::doublequote {text} {
    return \"[string map {\" {\"}} ${text}]\"
}

proc ::thtml::doublequote_and_escape_newlines {str {lengthVar ""}} {
    if { $lengthVar ne {} } {
        upvar $lengthVar length
        set length [string bytelength ${str}]
    }

    return [doublequote [string map {"\n" {\n} "\r" {\r} "\\" {\\}} ${str}]]
}

proc ::thtml::get_rootdir {} {
    variable rootdir
    if { ![info exists rootdir] || $rootdir eq {} } {
        set rootdir [file normalize [file dirname [info script]]]
    }
    return $rootdir
}

proc ::thtml::resolve_filepath {filepath} {

    set rootdir [get_rootdir]
    return [file normalize [file join $rootdir $filepath]]
}