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

proc ::thtml::util::starts_with {str prefix} {
    if { [string length $str] >= [string length $prefix] } {
        if { [string range $str 0 [expr { [string length $prefix] - 1 }]] eq $prefix } {
            return 1
        }
    }
    return 0
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
