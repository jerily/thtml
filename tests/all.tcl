package require tcltest
package require thtml
namespace import -force ::tcltest::test

::tcltest::configure -singleproc true -testdir [file dirname [info script]]

set tcl_tests_exit_code [::tcltest::runAllTests]

set rootdir [file normalize [file dirname [info script]]]
set target_lang c
::thtml::init [dict create cache 1 rootdir $rootdir target_lang $target_lang debug 0]
set cachedir [::thtml::get_cachedir]
file delete -force $cachedir
file mkdir $cachedir

::thtml::build::compiledir $rootdir/www $target_lang
::thtml::load_compiled_templates

set c_tests_exit_code [::tcltest::runAllTests]

exit [expr {$tcl_tests_exit_code + $c_tests_exit_code}]