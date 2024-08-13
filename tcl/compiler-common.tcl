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
        if { $tag in {tpl js css bundle_js bundle_css} } {
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
    } elseif { [$node tagName] eq {js} } {
        return [compile_statement_js codearr $node]
    } elseif { [$node tagName] eq {bundle_js} } {
        return [compile_statement_bundle_js codearr $node]
    } elseif { [$node tagName] eq {bundle_css} } {
        return [compile_statement_bundle_css codearr $node]
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

### js/css


proc ::thtml::compiler::compile_statement_bundle_css {codearrVar node} {
    upvar $codearrVar codearr
    set target_lang $codearr(target_lang)

    set top_component [::thtml::compiler::top_component codearr]
    set md5 [dict get $top_component md5]

    lappend codearr(bundle_metadata) md5 $md5

    set urlpath "${md5}/bundle_${md5}.css"

    append compiled_script "<link rel=\\\"stylesheet\\\" href=\\\""
    append compiled_script "\x03" [${target_lang}_compile_quoted_string codearr "\"[$node @url_prefix]\""] "\x02"
    append compiled_script "/${urlpath}\\\" />"
    return $compiled_script
}

proc ::thtml::compiler::compile_statement_bundle_js {codearrVar node} {
    upvar $codearrVar codearr
    set target_lang $codearr(target_lang)

    set top_component [::thtml::compiler::top_component codearr]
    set md5 [dict get $top_component md5]

    lappend codearr(bundle_metadata) md5 $md5

    set urlpath "${md5}/entry.js"

    append compiled_script "<script src=\\\""
    append compiled_script "\x03" [${target_lang}_compile_quoted_string codearr "\"[$node @url_prefix]\""] "\x02"
    append compiled_script "/${urlpath}\\\"></script>"
    return $compiled_script
}

proc ::thtml::compiler::compile_statement_js {codearrVar node} {
    upvar $codearrVar codearr
    set target_lang $codearr(target_lang)

    set js_num [incr codearr(js_count)]

    set top_component [::thtml::compiler::top_component codearr]
    set component_num [dict get $top_component component_num]

    set js [$node asText]
    set js_args [list]
    if [$node hasAttribute "args"] {
        foreach {name value} [$node @args {}] {
            lappend js_args $name
        }
    }

    lappend codearr(js_function,$component_num) $js_num $js_args $js
    lappend codearr(bundle_js_names) $component_num

#    append compiled_script "<script>THTML.com_${component_num}.js_${js_num}("
#    append compiled_script "\x03" [tcl_compile_quoted_string codearr "\"$js_vals\""] "\x02"
#    append compiled_script ");</script>"
#    return $compiled_script

    append codearr(js_code,$component_num) "\n" "com_${component_num}.js_${js_num}("
    append compiled_script "<script>"
    set first 1
    foreach {name value} [$node @args {}] {
        if { $first } {
            set first 0
        } else {
            append codearr(js_code,$component_num) ","
        }
        set argvar "js_${js_num}_arg_$name"
        append compiled_script "var ${argvar} = "
        append compiled_script "\x03" [${target_lang}_compile_quoted_string codearr "\"$value\""] "\x02"
        append compiled_script ";"
        append codearr(js_code,$component_num) "${argvar}"
    }
    append compiled_script "</script>"
    append codearr(js_code,$component_num) ");"

    return $compiled_script

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

proc ::thtml::compiler::get_seen {codearrVar what} {
    upvar $codearrVar codearr
    return [dict exists $codearr(seen) $what]
}

proc ::thtml::compiler::set_seen {codearrVar what} {
    upvar $codearrVar codearr
    set codearr(seen) [dict set codearr(seen) $what 1]
}

proc ::thtml::compiler::push_component {codearrVar component} {
    upvar $codearrVar codearr
    set codearr(components) [linsert $codearr(components) 0 $component]
}

proc ::thtml::compiler::pop_component {codearrVar} {
    upvar $codearrVar codearr
    set codearr(components) [lrange $codearr(components) 1 end]
}

proc ::thtml::compiler::top_component {codearrVar} {
    upvar $codearrVar codearr
    return [lindex $codearr(components) 0]
}

proc ::thtml::compiler::push_gc_list {codearrVar args} {
    upvar $codearrVar codearr
    set codearr(gc_lists) [linsert $codearr(gc_lists) 0 $args]
}

proc ::thtml::compiler::pop_gc_list {codearrVar} {
    upvar $codearrVar codearr
    set codearr(gc_lists) [lrange $codearr(gc_lists) 1 end]
}

proc ::thtml::compiler::top_gc_list {codearrVar} {
    upvar $codearrVar codearr
    return [lindex $codearr(gc_lists) 0]
}

proc ::thtml::compiler::lappend_gc_list {codearrVar args} {
    upvar $codearrVar codearr
    set top_gc_list [top_gc_list codearr]
    lappend top_gc_list {*}$args
    set codearr(gc_lists) [lreplace $codearr(gc_lists) 0 0 $top_gc_list]
}

proc ::thtml::compiler::lremove_gc_list {codearrVar what} {
    upvar $codearrVar codearr
    set top_gc_list [top_gc_list codearr]

    set result [list]
    foreach {type name} $top_gc_list {
        if { $name ne $what } {
            lappend result $type $name
        }
    }

    set codearr(gc_lists) [lreplace $codearr(gc_lists) 0 0 $result]
}
