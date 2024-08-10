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
    set md5 [::thtml::util::md5 $filepath]

    ::thtml::compiler::push_component codearr [list md5 $md5 dir [file dirname $filepath] component_num [incr codearr(component_count)]]

    set fp [open $filepath]
    set template [read $fp]
    close $fp

    set compiled_template [::thtml::compile codearr $template $target_lang]

    ::thtml::compiler::pop_component codearr

    return $compiled_template
}
