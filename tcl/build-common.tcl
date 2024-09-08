namespace eval ::thtml::build {}

proc ::thtml::build::compiledir {dir target_lang} {

    if { $target_lang ni {c tcl} } {
        error "Invalid target language: $target_lang"
    }

    return [${target_lang}_compiledir $dir]
}

proc ::thtml::build::compilefile {codearrVar filename target_lang} {
    variable ::thtml::debug
    upvar $codearrVar codearr

    if { $debug } { puts target_lang=$target_lang,compilefile=$filename }

    upvar $codearrVar codearr

    set filepath [::thtml::resolve_filepath codearr $filename]
    set relative_filepath [string range $filepath [string length [::thtml::get_rootdir]] end]
    set md5 [::thtml::util::md5 $relative_filepath]
    set compiled_template [::thtml::compilefile codearr $md5 $filepath $target_lang]
    return $compiled_template
}
