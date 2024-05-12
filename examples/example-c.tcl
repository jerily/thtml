package require thtml

set dir [file dirname [info script]]
set rootdir [file join $dir sample-blog]

::thtml::init [dict create cache 0 rootdir $rootdir]

set filepath [file join $rootdir "index.thtml"]
::thtml::compilefile $filepath