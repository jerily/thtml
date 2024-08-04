namespace eval ::thtml::bundle {}

proc ::thtml::bundle::process_bundle_js {codearrVar template_file_mtime} {
    variable ::thtml::debug
    upvar $codearrVar codearr

    if { [info exists codearr(bundle_metadata)] } {
        set bundle_metadata $codearr(bundle_metadata)
        set bundle_outdir [::thtml::get_bundle_outdir]
        set bundle_suffix [dict get $bundle_metadata suffix]
        set bundle_filename [file join $bundle_outdir "bundle_${bundle_suffix}.js"]
        if { !$debug && [file exists $bundle_filename] } {
            set bundle_mtime [file mtime $bundle_filename]
            puts bundle_mtime=$bundle_mtime,template_file_mtime=$template_file_mtime
            if { $bundle_mtime > $template_file_mtime } {
                return
            }
        }
        #puts bundle_filename=$bundle_filename

        ::thtml::bundle::build_bundle_js codearr $bundle_filename
    }
}

proc ::thtml::bundle::build_bundle_js {codearrVar bundle_filename} {
    upvar $codearrVar codearr

    if { [info exists codearr(bundle_js_names)] } {

        set rootdir [::thtml::get_rootdir]
        set cachedir [::thtml::get_cachedir]
        set node_modules_dir [file normalize [file join $rootdir node_modules]]

        set files_to_delete [list]
        set bundle_js_imports ""
        set bundle_js_exports ""
        set bundle_js_names $codearr(bundle_js_names)
        array set seen {}
        foreach bundle_js_name $bundle_js_names {
            # import_node_module adds md5 to codearr(bundle_js_names)
            # but also every js tag that uses the same bundle_js_name
            # we need to make sure we only process each bundle_js_name once
            if { [info exists seen($bundle_js_name)] } {
                continue
            }
            set seen($bundle_js_name) 1
            set component_js ""
            if { [info exists codearr(js_import,$bundle_js_name)] } {
                set js_imports ""
                foreach {name src} $codearr(js_import,$bundle_js_name) {
                    puts md5=$bundle_js_name,name=$name,src=$src
                    if { $name eq {} } {
                        append js_imports "\n" "import '$src';"
                    } else {
                        append js_imports "\n" "import $name from '$src';"
                    }
                }
                append component_js $js_imports
            }
            set component_exports [list]
            if { [info exists codearr(js_code,$bundle_js_name)] } {
                foreach {js_num js_args js} $codearr(js_code,$bundle_js_name) {
                    append component_js "\n" "function js_${bundle_js_name}_${js_num}(${js_args}) { ${js} }"
                    lappend component_exports "js_${bundle_js_name}_${js_num}"
                }
            }
            append component_js "\n" "export default \{"
            set first 1
            foreach component_export $component_exports {
                #puts $component_export
                if { $first } {
                    set first 0
                } else {
                    append component_js ","
                }
                append component_js "\n" "${component_export}"
            }
            append component_js "\n" "\};"
            set component_filename ${bundle_js_name}.js
            lappend files_to_delete [file join $cachedir $component_filename]
            writeFile [file join $cachedir $component_filename] $component_js
            append bundle_js_imports "\n" "import js_${bundle_js_name} from './${component_filename}';"
            lappend bundle_js_exports "js_${bundle_js_name}"
        }
        set entryfilename [file normalize [file join $cachedir "entry.js"]]
        lappend files_to_delete $entryfilename
        writeFile $entryfilename "${bundle_js_imports}\nexport default \{ [join ${bundle_js_exports} {,}] \};"
        if {[catch {
            set bundle_js [bundle_js $node_modules_dir $entryfilename]
        } errmsg]} {
            foreach filename $files_to_delete {
                file delete $filename
            }
            error "error in generating javascript bundle: $errmsg"
        }
        foreach filename $files_to_delete {
            file delete $filename
        }

        writeFile $bundle_filename $bundle_js
    }
}

proc ::thtml::bundle::bundle_js {node_modules_dir entryfile {name "THTML"}} {

    writeFile rollup.config.cjs [subst -nocommands -nobackslashes {
        module.exports = {
            input: '${entryfile}',
            output: {
                // file: 'mybundle.js',
                format: 'umd',
                name: '${name}',
                globals: {
//                    react: 'React',
//                    'react-dom': 'ReactDOM',
//                    redux: 'Redux',
//                    'react-redux': 'ReactRedux',
                },
            },
            plugins: [
                require('rollup-plugin-import-css')({
                    inject: true,
                }),
                // require('rollup-plugin-peer-deps-external')(),
                require('@rollup/plugin-node-resolve')({
                    modulePaths: ['${node_modules_dir}'],
                }),
                // commonjs plugin must be placed before babel plugin for the two to work together properly
                require('@rollup/plugin-commonjs')(),
                require('@rollup/plugin-babel')({
//                    exclude: 'node_modules/**',
                    babelHelpers: 'bundled',
                    // plugins: ['@babel/plugin-transform-runtime'],
                }),
                //require('rollup-plugin-postcss')({
                    //minimize: true,
                    // extract: true,
                    //plugins: [
                    //    require('cssnano')(),
                    //],
                //}),
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