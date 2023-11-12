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

proc ::thtml::util::resolve_filepath {filepath} {

    set rootdir [::thtml::get_rootdir]
    return [file normalize [file join $rootdir $filepath]]
}