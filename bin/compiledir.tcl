package require thtml

if { [llength $argv] != 4 } {
    puts "Usage: [file tail [info script]] <targetlang> <cachedir> <rootdir> <dir>"
    exit 1
}

set target_lang [lindex $argv 0]

if { $target_lang ni {c tcl}} {
    puts "Invalid target language: must be either 'c' or 'tcl'"
    exit 1
}

set cachedir [lindex $argv 1]
set rootdir [lindex $argv 2]
set dir [lindex $argv 3]

::thtml::init [dict create cache 1 rootdir $rootdir cachedir $cachedir target_lang $target_lang debug 1]

puts [::thtml::compiledir $dir $target_lang]