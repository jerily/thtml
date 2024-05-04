package require thtml

set dir [file dirname [info script]]
set rootdir [file join $dir sample-blog]

::thtml::init [dict create cache 0 rootdir $rootdir]

set filepath [file join $rootdir "index.thtml"]
set fp [open $filepath]
set template [read $fp]
close $fp
set c_code {}
append c_code "\n" "#include \"thtml.h\""
append c_code "\n" "int thtml_IndexPageCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {"
append c_code [::thtml::compile $template "c"]
append c_code "\n" "}"

puts $c_code