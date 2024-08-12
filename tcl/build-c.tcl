namespace eval ::thtml::build {}

proc ::thtml::build::c_compiledir {dir} {
    variable ::thtml::debug

    set target_lang "c"
    array set codearr [list blocks {} components {} target_lang $target_lang tcl_defs {} c_defs {} seen {} load_packages 1]

    set files [::thtml::util::find_files $dir "*.thtml"]
    if { $debug } { puts dir=$dir,files=$files }

    set compiled_cmds {}
    set compiled_code {}
    foreach file $files {
        set filepath [::thtml::resolve_filepath codearr $file]
        set filemd5 [::thtml::util::md5 $filepath]
        set proc_name ::thtml::cache::__file__$filemd5

        append compiled_code "\n" "// $filepath"
        append compiled_code "\n" "int thtml_${filemd5}Cmd(ClientData  clientData, Tcl_Interp *__interp__, int objc, Tcl_Obj * const objv\[\]) {"
        append compiled_code [compilefile codearr $file "c"]
        append compiled_code "\n" "}"
        append compiled_cmds "\n" "Tcl_CreateObjCommand(interp, \"${proc_name}\", thtml_${filemd5}Cmd, NULL, NULL);"

    }

    set dirpath [::thtml::resolve_filepath codearr $dir]
    set dirmd5 [::thtml::util::md5 $dirpath]

    set tcl_code $codearr(tcl_defs)
    tcl_build $dirmd5 $tcl_code

    set c_code "\#include \"thtml.h\"\n$codearr(c_defs)\n$compiled_code"

    set MIN_VERSION "9.0"
    append c_code "\n" "int Thtml_Init(Tcl_Interp *interp) {"
    append c_code "\n" "if (Tcl_InitStubs(interp, \"$MIN_VERSION\", 0) == NULL) { return TCL_ERROR; }"
    append c_code "\n" $compiled_cmds
    append c_code "\n" "return TCL_OK;"
    append c_code "\n" "}"

    if { $debug } { puts c_code=$c_code }

    return [c_build $dirmd5 $c_code]
}

proc ::thtml::build::c_build {dirmd5 c_code} {
    variable ::thtml::debug
    variable ::thtml::cachedir
    variable ::thtml::cmakedir

    if { $debug } { puts cachedir=$cachedir }

    set outfile [file join $cachedir "dir-$dirmd5.c"]
    set fp [open $outfile w]
    puts $fp $c_code
    close $fp

    set builddir [file join $cachedir "build"]
    cd $builddir
    set msgs [exec -ignorestderr -- cmake $cmakedir -DTHTML_CMAKE_DIR=$cmakedir -DTHTML_PROJECT_NAME=$dirmd5 -DTHTML_PROJECT_CODE=$outfile]
    if { $debug } { puts $msgs }
    set msgs [exec -ignorestderr -- make]
    if { $debug } { puts $msgs }
}