namespace eval ::thtml::compiler {
    variable EMPTY_ELEMENTS_IN_HTML {
        area base basefont br col frame
        hr img input isindex link meta param
    }
}

proc ::thtml::compiler::compile_helper {codearrVar node} {
    upvar $codearrVar codearr

    set node_type [$node nodeType]
    if { $node_type eq {TEXT_NODE} } {
        return [tcl_compile_template_text codearr \"[$node nodeValue]\"]
    } elseif { $node_type eq {ELEMENT_NODE} } {
        set tag [$node tagName]
        if { $tag eq {tpl} } {
            return [compile_statement codearr $node]
        } else {
            return [compile_element codearr $node]
        }
    }
}

proc ::thtml::compiler::compile_element {codearrVar node} {
    upvar $codearrVar codearr
    variable EMPTY_ELEMENTS_IN_HTML

    set tag [$node tagName]
    set compiled_element "<${tag}"
    foreach attname [$node attributes] {
        set attvalue [$node @$attname]
        set compiled_attvalue [tcl_compile_template_text codearr \"$attvalue\"]
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

proc ::thtml::compiler::compile_statement {codearrVar node} {
    upvar $codearrVar codearr
    set target_lang $codearr(target_lang)

    if { [$node hasAttribute "if"] } {
        return [${target_lang}_compile_statement_if codearr $node]
    } elseif { [$node hasAttribute "foreach"] } {
        return [${target_lang}_compile_statement_foreach codearr $node]
    } elseif { [$node hasAttribute "include"] } {
        return [${target_lang}_compile_statement_include codearr $node]
    } elseif { [$node hasAttribute "val"] } {
        return [${target_lang}_compile_statement_val codearr $node]
    } else {
        return [compile_children codearr $node]
    }
}


proc ::thtml::compiler::compile_children {codearrVar node} {
    upvar $codearrVar codearr

    set compiled_children ""
    foreach child [$node childNodes] {
        append compiled_children [compile_helper codearr $child]
    }
    return $compiled_children
}

proc ::thtml::compiler::compile_subst {codearrVar text inside_code_block} {
    upvar $codearrVar codearr

    set len [string length $text]
    set compiled_subst ""
    for {set i 0} {$i < $len} {incr i} {
        set ch [string index $text $i]
        if { $ch eq "\\" || $ch eq "\$" || $ch eq "\[" || $ch eq "\]" || $ch eq "\"" || $ch eq "\{" || $ch eq "\}" } {
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
                append compiled_subst [compile_subst_var codearr $varname $inside_code_block]
                set i $j
            }
        } else {
            append compiled_subst $ch
        }
    }
    return $compiled_subst
}

# when inside_code_block is false, we are inside a text block
# so we need to append the result of the substitution to the
# ds variable
proc ::thtml::compiler::compile_subst_var {codearrVar varname inside_code_block} {
    upvar $codearrVar codearr
    set target_lang $codearr(target_lang)

    set parts [split $varname "."]
    set parts_length [llength $parts]
    set varname_first_part [lindex $parts 0]
    foreach block $codearr(blocks) {
        foreach block_varname [dict get $block varnames] {
            if { ${block_varname} eq ${varname_first_part} } {
                if { $parts_length == 1 } {
                    return [${target_lang}_compile_subst_var_from_simple codearr $varname_first_part $inside_code_block]
                } else {
                    set varname_remaining_parts [lrange $parts 1 end]
                    return [${target_lang}_compile_subst_var_from_dict codearr $varname_first_part $varname_remaining_parts $inside_code_block]
                }
            }
        }
        if { [dict exists $block stop] } {
            # variable not found in blocks
            # break out of the loop and try
            # the __data__ dictionary of the
            # current evaluation context
            break
        }
    }
    return [${target_lang}_compile_subst_var_from_dict codearr __data__ $parts $inside_code_block]
}


### codearr manipulation

proc ::thtml::compiler::push_block {codearrVar block} {
    upvar $codearrVar codearr
    set codearr(blocks) [linsert $codearr(blocks) 0 $block]
}

proc ::thtml::compiler::pop_block {codearrVar} {
    upvar $codearrVar codearr
    set codearr(blocks) [lrange $codearr(blocks) 1 end]
}

proc ::thtml::compiler::top_block {codearrVar} {
    upvar $codearrVar codearr
    return [lindex $codearr(blocks) 0]
}
