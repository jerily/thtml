package require thtml

set dir [file dirname [info script]]
set rootdir [file join $dir sample-blog]

::thtml::init [dict create cache 0 rootdir $rootdir]

set filepath [file join $rootdir "index.thtml"]
set fp [open $filepath]
set template [read $fp]
close $fp
set tcl_code [::thtml::compile $template "tcl"]

puts $tcl_code