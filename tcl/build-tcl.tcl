namespace eval ::thtml::build {}

proc ::thtml::build::tcl_compiledir {dir} {
    variable ::thtml::debug

    set target_lang "tcl"
    array set codearr [list blocks {} components {} target_lang $target_lang gc_lists {} tcl_defs {} c_defs {} seen {} load_packages 1]

    if { $debug } { puts dir=$dir }
    set files [::thtml::util::find_files $dir "*.thtml"]
    if { $debug } { puts files=$files }

    set compiled_cmds {}
    set compiled_code {}
    foreach file $files {
        set filepath [::thtml::resolve_filepath codearr $file]
        set relative_filepath [string range $filepath [string length [::thtml::get_rootdir]] end]
        set filemd5 [::thtml::util::md5 $relative_filepath]
        set proc_name ::thtml::cache::__file__$filemd5

        append compiled_code "\n" "# $filepath"
        append compiled_code "\n" "proc ${proc_name} {__data__} {"
        append compiled_code [compilefile codearr $file "tcl"]
        append compiled_code "\n" "}"

    }

    set tcl_code "$codearr(tcl_defs)\n$compiled_code"

    set dirpath [::thtml::resolve_filepath codearr $dir]
    set relative_dirpath [string range $dirpath [string length [::thtml::get_rootdir]] end]
    set dirmd5 [::thtml::util::md5 $relative_dirpath]

    return [tcl_build $dirmd5 $tcl_code]
}

proc ::thtml::build::tcl_build {dirmd5 tcl_code} {
    variable ::thtml::debug

    set cachedir [::thtml::get_cachedir]
    if { $debug } { puts cachedir=$cachedir }

    set outfile [file normalize [file join $cachedir "dir-$dirmd5.tcl"]]
    set fp [open $outfile w]
    puts $fp $tcl_code
    close $fp

}
