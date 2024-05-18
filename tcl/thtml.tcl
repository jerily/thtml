# Copyright Jerily LTD. All Rights Reserved.
# SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
# SPDX-License-Identifier: MIT.

package require tdom

namespace eval ::thtml {
    variable rootdir
    variable cache 0
    variable target_lang tcl
    variable debug 0
}
namespace eval ::thtml::cache {}

proc ::thtml::init {option_dict} {
    variable rootdir
    variable cache
    variable target_lang
    variable debug

    if { [dict exists $option_dict rootdir] } {
        set rootdir [dict get $option_dict rootdir]
    }

    if { [dict exists $option_dict cache] } {
        set cache [dict get $option_dict cache]
    }

    if { [dict exists $option_dict target_lang] } {
        set target_lang [dict get $option_dict target_lang]
    }

    if { [dict exists $option_dict debug] } {
        set debug [dict get $option_dict debug]
    }
}

proc ::thtml::tcl_compile_template_and_load {codearrVar template} {
    upvar $codearrVar codearr

    set md5 [::thtml::util::md5 $template]
    set proc_name ::thtml::cache::__template__$md5

    set compiled_template [compile codearr $template tcl]
    eval $codearr(defs)
    proc $proc_name {__data__} "return \"<!doctype html>\[eval \{${compiled_template}\}\]\""

    #puts "$codearr(defs)\n$compiled_template"
}

proc ::thtml::c_compile_template_and_load {codearrVar template} {
    variable debug

    upvar $codearrVar codearr

    set md5 [::thtml::util::md5 $template]
    set proc_name ::thtml::cache::__template__$md5

    set compiled_template [compile codearr $template "c"]

    set c_code {}
    append c_code "\n" "#include \"thtml.h\""
    append c_code "\n" "#if TCL_MAJOR_VERSION > 8"
    append c_code "\n" "#define MIN_VERSION \"9.0\""
    append c_code "\n" "#else"
    append c_code "\n" "#define MIN_VERSION \"8.6\""
    append c_code "\n" "#endif"

    append c_code $codearr(defs)
    append c_code "\n" "int thtml_${md5}Cmd(ClientData  clientData, Tcl_Interp *__interp__, int objc, Tcl_Obj * const objv\[\]) {"
    append c_code $compiled_template
    append c_code "\n" "}"
    append c_code "\n" "int Thtml_Init(Tcl_Interp *interp) {"
    append c_code "\n" "if (Tcl_InitStubs(interp, MIN_VERSION, 0) == NULL) { return TCL_ERROR; }"
    append c_code "\n" "Tcl_CreateObjCommand(interp, \"${proc_name}\", thtml_${md5}Cmd, NULL, NULL);"
    append c_code "\n" "return TCL_OK;"
    append c_code "\n" "}"

    if { $debug } { puts c_code=$c_code }

    set dir [::thtml::get_rootdir]
    if { $debug } { puts rootdir=$dir }
    set cachedir [file normalize [file join $dir "cache/"]]
    if { $debug } { puts cachedir=$cachedir }
    set outfile [file join $cachedir "index.c"]
    set fp [open $outfile w]
    puts $fp $c_code
    close $fp

    set builddir [file join $cachedir "build"]
    cd $builddir
    set msgs [exec -ignorestderr -- cmake $cachedir -DTHTML_PROJECT_NAME=$md5 -DTHTML_PROJECT_CODE=index.c]
    if { $debug } { puts $msgs }
    set msgs [exec -ignorestderr -- make]
    if { $debug } { puts $msgs }

    load [file join $builddir "libthtml-${md5}.so"]
}

proc ::thtml::render {template __data__} {
    variable cache
    variable rootdir
    variable target_lang
    variable debug

    if { $debug } { puts target_lang=$target_lang }
    array set codearr [list blocks {} target_lang $target_lang defs {} seen {}]

    if { $cache } {
        set md5 [::thtml::util::md5 $template]
        set proc_name ::thtml::cache::__template__$md5
        if { [info proc $proc_name] eq {} } {
            ${target_lang}_compile_template_and_load codearr $template
        }
        return [$proc_name $__data__]
    }
    set compiled_template [compile codearr $template tcl]
    #puts compiled_template=$compiled_template
    eval $codearr(defs)
    return "<!doctype html>[eval $compiled_template]"
}

proc ::thtml::c_compile_file_and_load {codearrVar filename} {
    variable debug

    upvar $codearrVar codearr

    set filepath [::thtml::util::resolve_filepath $filename]

    set md5 [::thtml::util::md5 $filepath]
    set proc_name ::thtml::cache::__file__$md5

    set fp [open $filepath]
    set template [read $fp]
    close $fp

    set compiled_template [compile codearr $template "c"]

    set c_code {}
    append c_code "\n" "#include \"thtml.h\""
    append c_code "\n" "#if TCL_MAJOR_VERSION > 8"
    append c_code "\n" "#define MIN_VERSION \"9.0\""
    append c_code "\n" "#else"
    append c_code "\n" "#define MIN_VERSION \"8.6\""
    append c_code "\n" "#endif"

    append c_code $codearr(defs)
    append c_code "\n" "int thtml_${md5}Cmd(ClientData  clientData, Tcl_Interp *__interp__, int objc, Tcl_Obj * const objv\[\]) {"
    append c_code $compiled_template
    append c_code "\n" "}"
    append c_code "\n" "int Thtml_Init(Tcl_Interp *interp) {"
    append c_code "\n" "if (Tcl_InitStubs(interp, MIN_VERSION, 0) == NULL) { return TCL_ERROR; }"
    append c_code "\n" "Tcl_CreateObjCommand(interp, \"${proc_name}\", thtml_${md5}Cmd, NULL, NULL);"
    append c_code "\n" "return TCL_OK;"
    append c_code "\n" "}"

    if { $debug } { puts c_code=$c_code }

    set dir [::thtml::get_rootdir]
    if { $debug } { puts rootdir=$dir }
    set cachedir [file normalize [file join $dir "cache/"]]
    if { $debug } { puts cachedir=$cachedir }
    set outfile [file join $cachedir "index.c"]
    set fp [open $outfile w]
    puts $fp $c_code
    close $fp

    set builddir [file join $cachedir "build"]
    cd $builddir
    set msgs [exec -ignorestderr -- cmake $cachedir -DTHTML_PROJECT_NAME=$md5 -DTHTML_PROJECT_CODE=index.c]
    if { $debug } { puts $msgs }
    set msgs [exec -ignorestderr -- make]
    if { $debug } { puts $msgs }

    load [file join $builddir "libthtml-${md5}.so"]
}

proc ::thtml::tcl_compile_file_and_load {codearrVar filename} {
    upvar $codearrVar codearr

    set filepath [::thtml::util::resolve_filepath $filename]

    set md5 [::thtml::util::md5 $filepath]
    set proc_name ::thtml::cache::__file__$md5

    set fp [open $filepath]
    set template [read $fp]
    close $fp

    set compiled_template [compile codearr $template tcl]
    puts compiled_template=$codearr(defs)\n$compiled_template
    eval $codearr(defs)
    proc $proc_name {__data__} "return \"<!doctype html>\[eval \{${compiled_template}\}\]\""
}

proc ::thtml::renderfile {filename __data__} {
    variable cache
    variable rootdir
    variable target_lang

    array set codearr [list blocks {} target_lang $target_lang defs {} seen {}]

    set filepath [::thtml::util::resolve_filepath $filename]

    set fp [open $filepath]
    set template [read $fp]
    close $fp

    if { $cache } {
        set md5 [::thtml::util::md5 $filepath]
        set proc_name ::thtml::cache::__file__$md5
        if { [info proc $proc_name] eq {} } {
            ${target_lang}_compile_file_and_load codearr $filename
        }
        return [$proc_name $__data__]
    }

    set compiled_template [compile codearr $template tcl]
    #puts compiled_template=$compiled_template
    eval $codearr(defs)
    return "<!doctype html>[eval $compiled_template]"
}

proc ::thtml::compile {codearrVar template target_lang} {
    upvar $codearrVar codearr

    # TODO: temporary hack to escape special characters
    # this should be done in a more robust way
    # by escaping these characters when they are enclosed in square brackets
    # for expr commands, double quotes for if conditions, or val tags
    # set escaped_template [string map {{&&} {&amp;&amp;} {<} {&lt;} {>} {&gt;}} $template]
    set escaped_template [string map {{&&} {&amp;&amp;}} $template]
    dom parse -paramentityparsing never -- <root>$escaped_template</root> doc
    set root [$doc documentElement]

    rewrite $root

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

