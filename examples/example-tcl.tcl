package require thtml

set dir [file dirname [info script]]
set rootdir [file join $dir sample-blog]

::thtml::init [dict create cache 1 rootdir $rootdir]

set data [dict create title "My Index Page" content "Hello World!" path "/"]

set tcl_code [::thtml::renderfile index.thtml $data "tcl"]

puts $tcl_code