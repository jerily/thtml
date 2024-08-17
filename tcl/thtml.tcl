# Copyright Jerily LTD. All Rights Reserved.
# SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
# SPDX-License-Identifier: MIT.

package require tdom

namespace eval ::thtml {
    variable rootdir
    variable bundle_outdir
    variable bundle_css_outdir
    variable cachedir
    variable builddir
    variable cmakedir [file normalize [file join [file dirname [info script]] "../cmake"]]
    variable cache 0
    variable target_lang tcl
    variable debug 0
    variable build 0
}
namespace eval ::thtml::cache {}

proc ::thtml::init {option_dict} {
    variable rootdir
    variable bundle_outdir
    variable cachedir
    variable builddir
    variable cache
    variable target_lang
    variable debug
    variable build

    if { [dict exists $option_dict rootdir] } {
        set rootdir [file normalize [dict get $option_dict rootdir]]
        set cachedir [file normalize [file join $rootdir cache thtml]]
        set bundle_outdir [file normalize [file join $rootdir public bundle]]
    } else {
        error "rootdir is a required thtml config option"
    }

    if { [dict exists $option_dict bundle_outdir] } {
        set bundle_outdir [dict get $option_dict bundle_outdir]
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

    if { [dict exists $option_dict build] } {
        set build [dict get $option_dict build]
    }

    if { ![file isdirectory $cachedir] } {
        file mkdir $cachedir
    }

    if { ![file isdirectory $bundle_outdir] } {
        file mkdir $bundle_outdir
    }

    set builddir [file join $cachedir "build"]

    if { !$build && $cache } {
        load_compiled_templates
    }

}

proc ::thtml::get_cachedir {} {
    variable cachedir
    return $cachedir
}

proc ::thtml::get_builddir {} {
    variable builddir
    return $builddir
}

proc ::thtml::get_bundle_outdir {} {
    variable bundle_outdir
    return $bundle_outdir
}

proc ::thtml::get_bundle_css_outdir {} {
    variable bundle_css_outdir
    return $bundle_css_outdir
}

proc ::thtml::get_currentdir {codearrVar} {
    upvar $codearrVar codearr
    set top_component [::thtml::compiler::top_component codearr]
    set dir [dict get $top_component dir]
    return $dir
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

    set libdir [file normalize [file join $cachedir "build"]]
    set files [glob -nocomplain -directory $libdir libthtml-*[info sharedlibextension]]

    foreach file $files {
        if { $debug } { puts "loading $file" }
        load $file
        if { $debug } { puts "loaded $file" }
    }

    set libdir [file normalize $cachedir]
    set files [glob -nocomplain -directory $libdir dir-*.tcl]

    foreach file $files {
        if { $debug } { puts "loading $file" }
        source $file
        if { $debug } { puts "loaded $file" }
    }
}

proc ::thtml::render {template __data__} {
    variable cache
    variable rootdir
    variable target_lang
    variable debug

    if { $debug } { puts target_lang=$target_lang }
    array set codearr [list blocks {} components {} target_lang $target_lang gc_lists {} tcl_defs {} c_defs {} seen {} load_packages 0]

    set md5 [::thtml::util::md5 $template]

    if { $cache } {
        set proc_name ::thtml::cache::__template__$md5
        return [$proc_name $__data__]
    }
    set compiled_template [compile codearr $template tcl]
    #puts compiled_template=$compiled_template
    eval $codearr(tcl_defs)
    return "<!doctype html>[eval $compiled_template]"
}

proc ::thtml::renderfile {filename __data__} {
    variable cache
    variable rootdir
    variable target_lang

    array set codearr [list blocks {} components {} target_lang $target_lang gc_lists {} tcl_defs {} c_defs {} seen {} load_packages 0]

    set filepath [::thtml::resolve_filepath codearr $filename]

    set md5 [::thtml::util::md5 $filepath]

    if { $cache } {
        set proc_name ::thtml::cache::__file__$md5
        return [$proc_name $__data__]
    }

    set compiled_template [compilefile codearr $md5 $filepath tcl]
    #puts $codearr(tcl_defs)\ncompiled_template=$compiled_template
    eval $codearr(tcl_defs)
    return "<!doctype html>[eval $compiled_template]"
}

proc ::thtml::compile {codearrVar template target_lang} {
    upvar $codearrVar codearr

    set escaped_template [escape_template $template]
    dom parse -ignorexmlns -paramentityparsing never -- <root>$escaped_template</root> doc
    set root [$doc documentElement]

    process_node_module_imports codearr $root
    rewrite_template_imports codearr $root

    return [::thtml::compiler::${target_lang}_compile_root codearr $root]
}

proc ::thtml::compilefile {codearrVar md5 filepath target_lang} {
    upvar $codearrVar codearr

    set mtime [file mtime $filepath]

    set fp [open $filepath]
    set template [read $fp]
    close $fp

    ::thtml::compiler::push_component codearr [list md5 $md5 dir [file dirname $filepath] component_num [incr codearr(component_count)]]

    set result [compile codearr $template $target_lang]
    ::thtml::bundle::process_bundle codearr $mtime

    ::thtml::compiler::pop_component codearr

    return $result
}

proc ::thtml::process_node_module_imports {codearrVar root} {
    upvar $codearrVar codearr

    set top_component [::thtml::compiler::top_component codearr]
    set component_num [dict get $top_component component_num]

    set imports [$root getElementsByTagName import_node_module]
    set js_imports [list]
    foreach import $imports {
        set name [$import @name ""]
        set src [$import getAttribute src]

        set sep [file separator]
        set first_char [string index $src 0]
        set rootdir [::thtml::get_rootdir]
        if { $first_char eq $sep } {
            set src [file normalize ${rootdir}${src}]
            #puts src=$src
            if { ![::thtml::util::starts_with $src $rootdir] } {
                error "Invalid src: $src"
            }
        } elseif { $first_char eq {.} } {
            set currentdir [::thtml::get_currentdir codearr]
            set src [file normalize [file join $currentdir $src]]
            #puts src=$src
            if { ![::thtml::util::starts_with $src $rootdir] } {
                error "Invalid src: $src"
            }
        }


        lappend js_imports $name $src
        $import delete
    }

    lappend codearr(bundle_js_names) $component_num
    lappend codearr(js_import,$component_num) {*}${js_imports}

}

proc ::thtml::rewrite_template_imports {codearrVar root} {
    upvar $codearrVar codearr

    set imports [$root getElementsByTagName import]
    foreach import $imports {
        set src [$import getAttribute src]
        set name [$import getAttribute name]
        set filepath [::thtml::resolve_filepath codearr $src]

        # replace all occurrences of the imported name with include nodes
        set components [$root getElementsByTagName $name]
        foreach component $components {
            set pn [$component parentNode]

            # create include node
            set doc [$root ownerDocument]
            set include_node [$doc createElement tpl]
            $include_node setAttribute include $src

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

proc ::thtml::resolve_filepath {codearrVar filepath {currentdir ""}} {
    upvar $codearrVar codearr

    if { $filepath eq {} } {
        error "Empty filepath"
    }

    set sep [file separator]
    set first_char [string index $filepath 0]
    if { $first_char eq {@}} {
        set index [string first / $filepath]
        if { $index eq -1 } {
            error "Invalid filepath: $filepath"
        }
        set package_name [string range $filepath 1 [expr { $index - 1}]]

        if { $codearr(load_packages) } {
            package require $package_name
        }

        set dir [get_namespace_dir $package_name]
        set filepath [string range $filepath [expr { 1 + $index }] end]
        return [file normalize [file join $dir $filepath]]
    } elseif { $first_char eq $sep } {
        #puts $filepath
        set rootdir [file normalize [::thtml::get_rootdir]]
        if { [::thtml::util::starts_with $filepath $rootdir] } {
            return [file normalize $filepath]
        } else {
            return [file normalize ${rootdir}${filepath}]
        }
    }

    if { $currentdir eq {} } {
        set currentdir [file join [::thtml::get_rootdir] www]
    }
    return [file normalize [file join $currentdir $filepath]]
}

proc ::thtml::get_namespace_dir {nsp} {
    if { ![info exists ::${nsp}::__thtml__] } {
        error "Variable ::${nsp}::__thtml__ not found"
    }
    return [set ::${nsp}::__thtml__]
}
