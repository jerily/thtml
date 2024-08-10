# Copyright Jerily LTD. All Rights Reserved.
# SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
# SPDX-License-Identifier: MIT.

namespace eval ::thtml::util {}

proc ::thtml::util::doublequote {text} {
    return \"[string map {\" {\"}} ${text}]\"
}

proc ::thtml::util::doublequote_and_escape_newlines {str {lengthVar ""}} {
    if { $lengthVar ne {} } {
        upvar $lengthVar length
        set length [string bytelength ${str}]
    }

    return [doublequote [string map {"\n" {\n} "\r" {\r} "\\" {\\}} ${str}]]
}

proc ::thtml::util::get_namespace_dir {nsp} {
    if { ![info exists ::${nsp}::__thtml__] } {
        error "Variable ::${nsp}::__thtml__ not found"
    }
    return [set ::${nsp}::__thtml__]
}

proc ::thtml::util::resolve_filepath {filepath {currentdir ""}} {

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
        set nsp [string range $filepath 1 [expr { $index - 1}]]
        set dir [get_namespace_dir $nsp]
        set filepath [string range $filepath [expr { 1 + $index }] end]
        return [file normalize [file join $dir $filepath]]
    } elseif { $first_char eq $sep } {
        #puts $filepath
        set rootdir [::thtml::get_rootdir]
        return [file normalize ${rootdir}${filepath}]
    }

    if { $currentdir eq {} } {
        set currentdir [file join [::thtml::get_rootdir] www]
    }
    return [file normalize [file join $currentdir $filepath]]
}

# https://stackoverflow.com/questions/429386/tcl-recursively-search-subdirectories-to-source-all-tcl-files
proc ::thtml::util::find_files { basedir pattern } {

    # Fix the directory name, this ensures the directory name is in the
    # native format for the platform and contains a final directory seperator
    set basedir [string trimright [file join [file normalize $basedir] { }]]
    set fileList {}

    # Look in the current directory for matching files, -type {f r}
    # means ony readable normal files are looked at, -nocomplain stops
    # an error being thrown if the returned list is empty
    foreach fileName [glob -nocomplain -type {f r} -path $basedir $pattern] {
        lappend fileList $fileName
    }

    # Now look for any sub direcories in the current directory
    foreach dirName [glob -nocomplain -type {d  r} -path $basedir *] {
        # Recusively call the routine on the sub directory and append any
        # new files to the results
        set subDirList [find_files $dirName $pattern]
        if { [llength $subDirList] > 0 } {
            foreach subDirFile $subDirList {
                lappend fileList $subDirFile
            }
        }
    }
    return $fileList
}
