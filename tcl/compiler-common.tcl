# Copyright Jerily LTD. All Rights Reserved.
# SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
# SPDX-License-Identifier: MIT.

namespace eval ::thtml::compiler {
    variable EMPTY_ELEMENTS_IN_HTML {
        area base basefont br col frame
        hr img input isindex link meta param
    }
}

proc ::thtml::compiler::compile_helper {codearrVar node} {
    upvar $codearrVar codearr
    set target_lang $codearr(target_lang)

    set node_type [$node nodeType]
    if { $node_type eq {TEXT_NODE} } {
        return [${target_lang}_compile_template_text codearr \"[$node nodeValue]\"]
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
    set target_lang $codearr(target_lang)

    variable EMPTY_ELEMENTS_IN_HTML

    set tag [$node tagName]
    set compiled_element "<${tag}"
    foreach attname [$node attributes] {
        set attvalue [$node @$attname]
        set compiled_attvalue [${target_lang}_compile_template_text codearr \"$attvalue\"]
        append compiled_element " ${attname}=\\\"${compiled_attvalue}\\\""
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
