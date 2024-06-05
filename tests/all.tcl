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

set tcl_tests_exit_code [::tcltest::runAllTests]

set rootdir [file normalize [file dirname [info script]]]
set cachedir /tmp/cache/thtml
set target_lang c
::thtml::init [dict create cache 1 rootdir $rootdir cachedir $cachedir target_lang $target_lang debug 0]
::thtml::compiledir $rootdir/www $target_lang
::thtml::load_compiled_templates

set c_tests_exit_code [::tcltest::runAllTests]

exit [expr {$tcl_tests_exit_code + $c_tests_exit_code}]