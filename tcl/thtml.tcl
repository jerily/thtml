# Copyright Jerily LTD. All Rights Reserved.
# SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
# SPDX-License-Identifier: MIT.

package provide thtml 1.0.0

package require tdom

namespace eval ::thtml {
    variable EMPTY_ELEMENTS_IN_HTML {
        area base basefont br col frame
        hr img input isindex link meta param
    }
}

proc ::thtml::render {template data} {
    set compiled_template [compile $template]
    return [eval $compiled_template]
}

proc ::thtml::compile {template} {
    dom parse $template doc
    set root [$doc documentElement]
    set compiled_template [transform \x02[compile_helper $root]\x03]
}

# special characters that denote the start and end of html Vs code blocks:
# \x02 - start of text
# \x03 - end of text
proc ::thtml::transform {intermediate_code} {
    set compiled_template ""

    set re {\x02([^\x02\x03]*)\x03}
    set start 0
    while {[regexp -start $start -indices -- $re $intermediate_code match submatch]} {
        lassign $submatch subStart subEnd
        lassign $match matchStart matchEnd
        incr matchStart -1
        incr matchEnd

        set before_text [string range $intermediate_code $start $matchStart]
        if { $before_text ne {} } {
            append compiled_template " " $before_text
        }

        set in_text [string range $intermediate_code $subStart $subEnd]
        if { $in_text ne {} } {
            set bytes [doublequote_and_escape_newlines $in_text length]
            append compiled_template "\n" "append ds ${bytes}" "\n"
        }

        set start $matchEnd
    }
    set after_text [string range $text $start end]
    if { $after_text ne {} } {
	    append compiled_template "\n" $after_text
    }

    return $compiled_template
}

proc ::thtml::compile_helper {node} {
    set node_type [$node nodeType]
    if { $node_type eq {TEXT_NODE} } {
        return [compile_subst [$node nodeValue]]
    } elseif { $node_type eq {ELEMENT_NODE} } {
        set tag [$node tagName]
        if { $tag eq {tpl} } {
            return [compile_statement $node]
        } else {
            return [compile_element $node]
        }
    }
}

proc ::thtml::compile_element {node} {
    variable EMPTY_ELEMENTS_IN_HTML
    set tag [$node tagName]
    set compiled_element "<${tag}"
    foreach attname [$node attributes] {
        set attvalue [$node @$attname]
        set compiled_attvalue [compile_subst $attvalue]
        append compiled_element " ${attname}=\"${compiled_attvalue}\""
    }

    if { $tag in $EMPTY_ELEMENTS_IN_HTML } {
        append compiled_element "/>"
        set ctag ""
    } else {
        append compiled_element ">"
        set ctag "</${tag}>"
    }

    append compiled_element [compile_children $node]
    append compiled_element $ctag
    return $compiled_element
}

proc ::thtml::compile_statement {node} {
    if { [$node hasAttribute "if"] } {
        return [compile_statement_if $node]
    } elseif { [$node hasAttribute "for"] } {
        return [compile_statement_for $node]
    } elseif { [$node hasAttribute "include"] } {
        return [compile_statement_include $node]
    } else {
        return [compile_children $node]
    }
}

proc ::thtml::compile_statement_if {node} {
    set conditional [$node @if]
    set compiled_conditional [compile_statement_if_expr $conditional]

    set compiled_statement ""
    append compiled_statement "\x03" "\n" "if \{ $compiled_conditional \} \{ " "\x02"
    append compiled_statement [compile_children $node]
    append compiled_statement "\x03" "\n" "\} " "\x02"
    return $compiled_statement
}

proc ::thtml::compile_statement_if_expr {conditional} {
    # todo: implement
}

proc ::thtml::compile_statement_for {node} {
    # todo: implement
}

proc ::thtml::compile_statement_include {node} {
    # todo: implement
}

proc ::thtml::doublequote {text} {
    return \"[string map {\" {\"}} ${text}]\"
}

proc ::thtml::doublequote_and_escape_newlines {str {lengthVar ""}} {
    if { $lengthVar ne {} } {
        upvar $lengthVar length
        set length [string bytelength ${str}]
    }

    return [::util::doublequote [string map {"\n" {\n} "\r" {\r} "\\" {\\}} ${str}]]
}

