package require thtml

if { [llength $argv] != 3 } {
    puts "Usage: [file tail [info script]] <targetlang> <rootdir> <dir>"
    exit 1
}

set target_lang [lindex $argv 0]

if { $target_lang ni {c tcl}} {
    puts "Invalid target language: must be either 'c' or 'tcl'"
    exit 1
}

set rootdir [lindex $argv 1]
set dir [lindex $argv 2]

::thtml::init [dict create cache 1 rootdir $rootdir target_lang $target_lang debug 1]

puts [::thtml::compiledir $dir $target_lang]