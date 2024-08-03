# Copyright Jerily LTD. All Rights Reserved.
# SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
# SPDX-License-Identifier: MIT.

package require tdom

namespace eval ::thtml {
    variable rootdir
    variable bundle_outdir
    variable cachedir /tmp/cache/thtml
    variable cmakedir [file normalize [file join [file dirname [info script]] "../cmake"]]
    variable cache 0
    variable target_lang tcl
    variable debug 0
}
namespace eval ::thtml::cache {}

proc ::thtml::init {option_dict} {
    variable rootdir
    variable bundle_outdir
    variable cachedir
    variable cache
    variable target_lang
    variable debug

    if { [dict exists $option_dict rootdir] } {
        set rootdir [dict get $option_dict rootdir]
    }

    if { [dict exists $option_dict bundle_outdir] } {
        set bundle_outdir [dict get $option_dict bundle_outdir]
    }

    if { [dict exists $option_dict cachedir] } {
        set cachedir [dict get $option_dict cachedir]
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

    if { ![file isdirectory $cachedir] } {
        file mkdir $cachedir
    }

    set builddir [file join $cachedir "build"]
    if { ![file isdirectory $builddir] } {
        file mkdir $builddir
    }

}

proc ::thtml::get_cachedir {} {
    variable cachedir
    return $cachedir
}

proc ::thtml::get_bundle_outdir {} {
    variable bundle_outdir
    return $bundle_outdir
}

proc ::thtml::compiledir {dir target_lang} {

    if { $target_lang ni {c tcl} } {
        error "Invalid target language: $target_lang"
    }

    return [${target_lang}_compiledir $dir]
}

proc ::thtml::c_compiledir {dir} {
    variable debug

    set target_lang "c"
    array set codearr [list blocks {} components {} target_lang $target_lang defs {} seen {}]

    set files [::thtml::util::find_files $dir "*.thtml"]
    if { $debug } { puts files=$files }

    set compiled_cmds {}
    set compiled_code {}
    foreach file $files {
        set filepath [::thtml::util::resolve_filepath $file]
        set filemd5 [::thtml::util::md5 $filepath]
        set proc_name ::thtml::cache::__file__$filemd5

        append compiled_code "\n" "// $filepath"
        append compiled_code "\n" "int thtml_${filemd5}Cmd(ClientData  clientData, Tcl_Interp *__interp__, int objc, Tcl_Obj * const objv\[\]) {"
        append compiled_code [compilefile codearr $file "c"]
        append compiled_code "\n" "}"
        append compiled_cmds "\n" "Tcl_CreateObjCommand(interp, \"${proc_name}\", thtml_${filemd5}Cmd, NULL, NULL);"

    }

    set dirpath [::thtml::util::resolve_filepath $dir]
    set dirmd5 [::thtml::util::md5 $dirpath]

    set c_code "\#include \"thtml.h\"\n$codearr(defs)\n$compiled_code"

    set MIN_VERSION "9.0"
    append c_code "\n" "int Thtml_Init(Tcl_Interp *interp) {"
    append c_code "\n" "if (Tcl_InitStubs(interp, \"$MIN_VERSION\", 0) == NULL) { return TCL_ERROR; }"
    append c_code "\n" $compiled_cmds
    append c_code "\n" "return TCL_OK;"
    append c_code "\n" "}"

    if { $debug } { puts c_code=$c_code }

    return [c_build $dirmd5 $c_code]
}

proc ::thtml::c_build {dirmd5 c_code} {
    variable debug
    variable cachedir
    variable cmakedir

    if { $debug } { puts cachedir=$cachedir }

    set outfile [file join $cachedir "dir-$dirmd5.c"]
    set fp [open $outfile w]
    puts $fp $c_code
    close $fp

    set builddir [file join $cachedir "build"]
    cd $builddir
    set msgs [exec -ignorestderr -- cmake $cmakedir -DTHTML_CMAKE_DIR=$cmakedir -DTHTML_PROJECT_NAME=$dirmd5 -DTHTML_PROJECT_CODE=$outfile]
    if { $debug } { puts $msgs }
    set msgs [exec -ignorestderr -- make]
    if { $debug } { puts $msgs }
}

proc ::thtml::tcl_compiledir {dir} {
    variable debug

    set target_lang "tcl"
    array set codearr [list blocks {} components {} target_lang $target_lang defs {} seen {}]

    set files [::thtml::util::find_files $dir "*.thtml"]
    if { $debug } { puts files=$files }

    set compiled_cmds {}
    set compiled_code {}
    foreach file $files {
        set filepath [::thtml::util::resolve_filepath $file]
        set filemd5 [::thtml::util::md5 $filepath]
        set proc_name ::thtml::cache::__file__$filemd5

        append compiled_code "\n" "# $filepath"
        append compiled_code "\n" "proc ${proc_name} {__data__} {"
        append compiled_code [compilefile codearr $file "tcl"]
        append compiled_code "\n" "}"

    }

    set tcl_code "$codearr(defs)\n$compiled_code"

    set dirpath [::thtml::util::resolve_filepath $dir]
    set dirmd5 [::thtml::util::md5 $dirpath]

    return [tcl_build $dirmd5 $tcl_code]
}

proc ::thtml::tcl_build {dirmd5 tcl_code} {
    variable debug
    variable cachedir

    if { $debug } { puts cachedir=$cachedir }

    set outfile [file normalize [file join $cachedir "dir-$dirmd5.tcl"]]
    set fp [open $outfile w]
    puts $fp $tcl_code
    close $fp

}

proc ::thtml::load_compiled_templates {} {
    variable debug
    variable cache
    variable cachedir
    variable target_lang

    if { !$cache } {
        puts "load_compiled_templates called with cache disabled, returning..."
        return
    }

    if { $target_lang eq {c} } {
        set libdir [file normalize [file join $cachedir "build"]]
        set files [glob -nocomplain -directory $libdir libthtml-*[info sharedlibextension]]
    } else {
        set libdir [file normalize $cachedir]
        set files [glob -nocomplain -directory $libdir dir-*.tcl]
    }
    foreach file $files {
        if { $debug } { puts "loading $file" }
        if { $target_lang eq {c} } {
            load $file
        } else {
            source $file
        }
        if { $debug } { puts "loaded $file" }
    }
}

proc ::thtml::compilefile {codearrVar filename target_lang} {
    variable debug

    if { $debug } { puts target_lang=$target_lang,compilefile=$filename }

    upvar $codearrVar codearr

    set filepath [::thtml::util::resolve_filepath $filename]

    set fp [open $filepath]
    set template [read $fp]
    close $fp

    set compiled_template [compile codearr $template $target_lang]
    return $compiled_template
}

proc ::thtml::render {template __data__} {
    variable cache
    variable rootdir
    variable target_lang
    variable debug

    if { $debug } { puts target_lang=$target_lang }
    array set codearr [list blocks {} components {} target_lang $target_lang defs {} seen {}]

    set md5 [::thtml::util::md5 $template]
    ::thtml::compiler::push_component codearr [list md5 $md5]

    if { $cache } {
        set proc_name ::thtml::cache::__template__$md5
        return [$proc_name $__data__]
    }
    set compiled_template [compile codearr $template tcl]
    #puts compiled_template=$compiled_template
    eval $codearr(defs)
    return "<!doctype html>[eval $compiled_template]"
}

proc ::thtml::renderfile {filename __data__} {
    variable cache
    variable rootdir
    variable target_lang

    array set codearr [list blocks {} components {} target_lang $target_lang defs {} seen {}]

    set filepath [::thtml::util::resolve_filepath $filename]
    set mtime [file mtime $filepath]

    set fp [open $filepath]
    set template [read $fp]
    close $fp

    set md5 [::thtml::util::md5 $filepath]
    ::thtml::compiler::push_component codearr [list md5 $md5]

    if { $cache } {
        set proc_name ::thtml::cache::__file__$md5
        return [$proc_name $__data__]
    }

    set compiled_template [compile codearr $template tcl]
    #puts $codearr(defs)\ncompiled_template=$compiled_template
    eval $codearr(defs)
    process_bundle_js codearr $mtime
    return "<!doctype html>[eval $compiled_template]"
}

proc ::thtml::build_bundle_js {codearrVar bundle_filename} {
    upvar $codearrVar codearr

    if { [info exists codearr(bundle_js_names)] } {
        set cachedir .
        set files_to_delete [list]
        set bundle_js_imports ""
        set bundle_js_exports ""
        foreach bundle_js_name $codearr(bundle_js_names) {
            set component_js ""
            if { [info exists codearr(js_import,$bundle_js_name)] } {
                append component_js $codearr(js_import,$bundle_js_name)
            }
            if { [info exists codearr(js_code,$bundle_js_name)] } {
                set component_exports [list]
                foreach {js_num js_args js} $codearr(js_code,$bundle_js_name) {
                    append component_js "\n" "function js_${bundle_js_name}_${js_num}(${js_args}) { ${js} }"
                    lappend component_exports "js_${bundle_js_name}_${js_num}"
                }
                append component_js "\n" "export default \{"
                set first 1
                foreach component_export $component_exports {
                    puts $component_export
                    if { $first } {
                        set first 0
                    } else {
                        append component_js ","
                    }
                    append component_js "\n" "${component_export}"
                }
                append component_js "\};"
            }
            set component_filename [file join $cachedir ${bundle_js_name}.js]
            lappend files_to_delete $component_filename
            writeFile $component_filename $component_js
            append bundle_js_imports "\n" "import js_${bundle_js_name} from '${component_filename}';"
            lappend bundle_js_exports "js_${bundle_js_name}"
        }
        set entryfilename [file join $cachedir "entry.js"]
        lappend files_to_delete $entryfilename
        writeFile $entryfilename "${bundle_js_imports}\nexport default \{ [join ${bundle_js_exports} {,}] \};"
        if {[catch {
            set bundle_js [::thtml::util::bundle_js $entryfilename]
        }]} {
            puts stderr "error in generating javascript bundle"
        }
        foreach filename $files_to_delete {
            file delete $filename
        }

        writeFile $bundle_filename $bundle_js
    }
}

proc ::thtml::process_bundle_js {codearrVar template_file_mtime} {
    upvar $codearrVar codearr

    if { [info exists codearr(bundle_metadata)] } {
        set bundle_metadata $codearr(bundle_metadata)
        set bundle_outdir [::thtml::get_bundle_outdir]
        set bundle_suffix [dict get $bundle_metadata suffix]
        set bundle_filename [file join $bundle_outdir "bundle_${bundle_suffix}.js"]
        if { [file exists $bundle_filename] } {
            set bundle_mtime [file mtime $bundle_filename]
            puts bundle_mtime=$bundle_mtime,template_file_mtime=$template_file_mtime
            if { $bundle_mtime > $template_file_mtime } {
                return
            }
        }
        #puts bundle_filename=$bundle_filename

        build_bundle_js codearr $bundle_filename
    }
}

proc ::thtml::compile {codearrVar template target_lang} {
    upvar $codearrVar codearr

    set escaped_template [escape_template $template]
    dom parse -ignorexmlns -paramentityparsing never -- <root>$escaped_template</root> doc
    set root [$doc documentElement]

    rewrite_template_imports $root
    process_node_module_imports codearr $root

    return [::thtml::compiler::${target_lang}_compile_root codearr $root]
}

proc ::thtml::process_node_module_imports {codearrVar root} {
    upvar $codearrVar codearr

    set top_component [::thtml::compiler::top_component codearr]
    set md5 [dict get $top_component md5]

    set imports [$root getElementsByTagName import_node_module]
    set js_imports ""
    foreach import $imports {
        set src [$import getAttribute src]
        set name [$import getAttribute name]
        append js_imports "\n" "import $name from '$src';"
        $import delete
    }

    append codearr(js_import,$md5) ${js_imports}

}

proc ::thtml::rewrite_template_imports {root} {
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
    return [file normalize $rootdir]
}

