namespace eval ::thtml::runtime::tcl {}

proc ::thtml::runtime::tcl::evaluate_script {script} {
    return [uplevel 1 $script]
}

