# Copyright Jerily LTD. All Rights Reserved.
# SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
# SPDX-License-Identifier: MIT.

namespace eval ::thtml::util {}

proc ::thtml::util::doublequote {text} {
    return \"[string map {\" {\"}} ${text}]\"
}

proc ::thtml::util::doublequote_and_escape_newlines {str {lengthVar ""}} {
    if { $lengthVar ne {} } {
        upvar $lengthVar length
        set length [string bytelength ${str}]
    }

    return [doublequote [string map {"\n" {\n} "\r" {\r} "\\" {\\}} ${str}]]
}

proc ::thtml::util::get_namespace_dir {nsp} {
    if { ![info exists ::${nsp}::__thtml__] } {
        error "Variable ::${nsp}::__thtml__ not found"
    }
    return [set ::${nsp}::__thtml__]
}

proc ::thtml::util::resolve_filepath {filepath} {

    if { $filepath eq {} } {
        error "Empty filepath"
    }

    set first_char [string index $filepath 0]
    if { $first_char eq {@}} {
        set index [string first / $filepath]
        if { $index eq -1 } {
            error "Invalid filepath: $filepath"
        }
        set nsp [string range $filepath 1 [expr { $index - 1}]]
        set dir [get_namespace_dir $nsp]
        set filepath [string range $filepath [expr { 1 + $index }] end]
        return [file normalize [file join $dir $filepath]]
    }

    set rootdir [::thtml::get_rootdir]
    return [file normalize [file join $rootdir www $filepath]]
}

# https://stackoverflow.com/questions/429386/tcl-recursively-search-subdirectories-to-source-all-tcl-files
proc ::thtml::util::find_files { basedir pattern } {

    # Fix the directory name, this ensures the directory name is in the
    # native format for the platform and contains a final directory seperator
    set basedir [string trimright [file join [file normalize $basedir] { }]]
    set fileList {}

    # Look in the current directory for matching files, -type {f r}
    # means ony readable normal files are looked at, -nocomplain stops
    # an error being thrown if the returned list is empty
    foreach fileName [glob -nocomplain -type {f r} -path $basedir $pattern] {
        lappend fileList $fileName
    }

    # Now look for any sub direcories in the current directory
    foreach dirName [glob -nocomplain -type {d  r} -path $basedir *] {
        # Recusively call the routine on the sub directory and append any
        # new files to the results
        set subDirList [findFiles $dirName $pattern]
        if { [llength $subDirList] > 0 } {
            foreach subDirFile $subDirList {
                lappend fileList $subDirFile
            }
        }
    }
    return $fileList
}

proc ::thtml::util::bundle_js {entryfile} {

    writeFile rollup.config.cjs [subst -nocommands -nobackslashes {
        module.exports = {
            input: '${entryfile}',
            output: {
                // file: 'mybundle.js',
                format: 'umd',
                name: 'mybundle',
                globals: {
//                    react: 'React',
//                    'react-dom': 'ReactDOM',
//                    redux: 'Redux',
//                    'react-redux': 'ReactRedux',
                },
            },
            plugins: [
                // require('rollup-plugin-peer-deps-external')(),
                require('@rollup/plugin-node-resolve')(),
                // commonjs plugin must be placed before babel plugin for the two to work together properly
                require('@rollup/plugin-commonjs')(),
                require('@rollup/plugin-babel')({
//                    exclude: 'node_modules/**',
                    babelHelpers: 'bundled',
                    // plugins: ['@babel/plugin-transform-runtime'],
                }),
                require('rollup-plugin-postcss')({
                    minimize: true,
                    // extract: true,
                    plugins: [
                        require('cssnano')(),
                    ],
                }),
                // require('@rollup/plugin-terser')(),
            ],
            external: [
                //'react',
                //'react-dom',
                //'redux',
                //'react-redux',
                 // /@babel\/runtime/,
            ],
        };
    }]

    if { [catch {set bundle_js [exec -ignorestderr -- npx --no-install rollup -c rollup.config.cjs]} errmsg] } {
        error "rollup error: $errmsg"
    }
    file delete rollup.config.cjs
    return $bundle_js
}