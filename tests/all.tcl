package require tcltest
namespace import -force ::tcltest::test

if { [llength $argv] != 1 } {
    puts stderr "Usage: $argv0 libdir"
    exit 1
}

lassign $argv libdir

set auto_path [linsert $auto_path 0 $libdir]
set argv [lrange $argv 1 end]

::tcltest::configure -singleproc true -testdir [file dirname [info script]]

exit [::tcltest::runAllTests]