# Copyright Jerily LTD. All Rights Reserved.
# SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
# SPDX-License-Identifier: MIT.

package require tdom

namespace eval ::thtml {
    variable rootdir
    variable cache 0
}
namespace eval ::thtml::cache {}

proc ::thtml::init {option_dict} {
    variable rootdir
    variable cache

    if { [dict exists $option_dict rootdir] } {
        set rootdir [dict get $option_dict rootdir]
    }

    if { [dict exists $option_dict cache] } {
        set cache [dict get $option_dict cache]
    }
}

proc ::thtml::render {template __data__} {
    variable cache
    variable rootdir
    if { $cache } {
        set md5 [::thtml::util::md5 $template]
        set proc_name ::thtml::cache::__render__$md5
        if { [info proc $proc_name] eq {} } {
            set compiled_template [compile $template tcl]
            proc $proc_name {__data__} "return \"<!doctype html>\[eval \{${compiled_template}\}\]\""
        }
        return [$proc_name $__data__]
    }
    set compiled_template [compile $template tcl]
    #puts compiled_template=$compiled_template
    return "<!doctype html>[eval $compiled_template]"
}

proc ::thtml::renderfile {filename __data__} {
    variable cache
    variable rootdir

    set filepath [::thtml::util::resolve_filepath $filename]

    set fp [open $filepath]
    set template [read $fp]
    close $fp

    if { $cache } {
        set md5 [::thtml::util::md5 $filepath]
        set proc_name ::thtml::cache::__renderfile__$md5
        if { [info proc $proc_name] eq {} } {
            set compiled_template [compile $template tcl]
            proc $proc_name {__data__} "return \"<!doctype html>\[eval \{${compiled_template}\}\]\""
        }
        return [$proc_name $__data__]
    }

    set compiled_template [compile $template tcl]
    #puts compiled_template=$compiled_template
    return "<!doctype html>[eval $compiled_template]"
}

proc ::thtml::compile {template target_lang} {
    set escaped_template [string map {{&&} {&amp;&amp;}} $template]
    dom parse -paramentityparsing never -- $escaped_template doc
    set root [$doc documentElement]

    rewrite $root

    array set codearr [list blocks {} target_lang $target_lang]
    return [::thtml::compiler::${target_lang}_compile_root codearr $root]
}

proc ::thtml::rewrite {root} {
    set imports [$root getElementsByTagName import]
    foreach import $imports {
        set src [$import getAttribute src]
        set name [$import getAttribute name]
        set filepath [::thtml::util::resolve_filepath $src]

        # replace all occurrences of the imported name with include nodes
        set components [$root getElementsByTagName $name]
        foreach component $components {
            set pn [$component parentNode]

            # create include node
            set doc [$root ownerDocument]
            set include_node [$doc createElement tpl]
            $include_node setAttribute include $filepath

            # copy attributes and children
            foreach attname [$component attributes] {
                $include_node setAttribute $attname [$component getAttribute $attname]
            }
            foreach child [$component childNodes] {
                $include_node appendChild $child
            }

            # replace component with include node
            $pn insertBefore $include_node $component
            $component delete
        }

        # remove import node
        $import delete
    }
}

### runtime

proc ::thtml::get_rootdir {} {
    variable rootdir
    if { ![info exists rootdir] || $rootdir eq {} } {
        set rootdir [file normalize [file dirname [info script]]]
    }
    return $rootdir
}

