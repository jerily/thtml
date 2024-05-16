package require tcltest
namespace import -force ::tcltest::test

if { [llength $argv] != 2 } {
    puts stderr "Usage: $argv0 libdir target_lang"
    exit 1
}

lassign $argv libdir target_lang

if { $target_lang ni {tcl c}} {
    puts stderr "Invalid target_lang: $target_lang"
    exit 1
}

set auto_path [linsert $auto_path 0 $libdir]
set argv [lrange $argv 2 end]

::tcltest::configure -singleproc true -testdir [file dirname [info script]]

exit [::tcltest::runAllTests]