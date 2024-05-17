package require thtml

set dir [file dirname [info script]]
set rootdir [file join $dir sample-blog]

::thtml::init [dict create cache 1 rootdir $rootdir target_lang "c" debug 1]

set data [dict create title "My Index Page" content "Hello World!" path "/"]

puts [::thtml::renderfile "index.thtml" $data]