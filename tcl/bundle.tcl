namespace eval ::thtml::bundle {}

proc ::thtml::bundle::process_bundle {codearrVar template_file_mtime} {
    variable ::thtml::debug
    upvar $codearrVar codearr

    if { [info exists codearr(bundle_metadata)] } {
        set bundle_metadata $codearr(bundle_metadata)
        set bundle_outdir [::thtml::get_bundle_outdir]
        set bundle_md5 [dict get $bundle_metadata md5]
        set bundle_js_filepath [file normalize [file join $bundle_outdir $bundle_md5 "entry.js"]]
        set bundle_css_filepath [file normalize [file join $bundle_outdir $bundle_md5 "bundle_${bundle_md5}.css"]]
        if { !$debug && [file exists $bundle_js_filepath] && [file exists $bundle_css_filepath] } {
            set bundle_js_mtime [file mtime $bundle_js_filepath]
            set bundle_css_mtime [file mtime $bundle_css_filepath]
            if { $bundle_js_mtime > $template_file_mtime && $bundle_css_mtime > $template_file_mtime } {
                return
            }
        }

        ::thtml::bundle::make_bundle codearr $bundle_outdir $bundle_md5
    }
}

proc ::thtml::bundle::make_bundle {codearrVar bundle_outdir bundle_md5} {
    upvar $codearrVar codearr

    if { [info exists codearr(bundle_js_names)] } {

        set rootdir [::thtml::get_rootdir]
        set cachedir [::thtml::get_cachedir]
        set node_modules_dir [file normalize [file join $rootdir node_modules]]

        set files_to_delete [list]
        set bundle_js_imports ""
        set bundle_js_code ""
#        set bundle_js_exports ""
        set bundle_js_names $codearr(bundle_js_names)
        array set seen {}
        foreach bundle_js_name $bundle_js_names {
            # import_node_module adds component_num to codearr(bundle_js_names)
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
                    #puts component_num=$bundle_js_name,name=$name,src=$src
                    if { $name eq {} } {
                        append js_imports "\n" "import '$src';"
                    } else {
                        append js_imports "\n" "import $name from '$src';"
                    }
                }
                append component_js $js_imports
            }
            set component_exports [list]
            if { [info exists codearr(js_function,$bundle_js_name)] } {
                foreach {js_num js_args js} $codearr(js_function,$bundle_js_name) {
                    append component_js "\n" "function js_${js_num}([join ${js_args} {,}]) { ${js} }"
                    lappend component_exports "js_${js_num}"
                }
            }
            if { [info exists codearr(js_code,$bundle_js_name)] } {
                append bundle_js_code "\n" $codearr(js_code,$bundle_js_name)
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
            set component_name "com_${bundle_js_name}"
            append bundle_js_imports "\n" "import ${component_name} from './${component_filename}';"
#            lappend bundle_js_exports "${component_name}"
        }
        set entryfilename [file normalize [file join $cachedir "entry.js"]]
        lappend files_to_delete $entryfilename
#        writeFile $entryfilename "${bundle_js_imports}\n${bundle_js_code}\nexport default \{ [join ${bundle_js_exports} {,}] \};"
        writeFile $entryfilename "${bundle_js_imports}\n${bundle_js_code}"
        if {[catch {
            set bundle_js [invoke_rollup $node_modules_dir $entryfilename $bundle_outdir $bundle_md5]
        } errmsg]} {
            foreach filename $files_to_delete {
                file delete $filename
            }
            error "error in generating javascript bundle: $errmsg"
        }
        foreach filename $files_to_delete {
            file delete $filename
        }
    }
}

proc ::thtml::bundle::invoke_rollup {node_modules_dir entryfile bundle_outdir bundle_md5 {name "THTML"}} {

    set rollup_outdir [file join $bundle_outdir $bundle_md5]
#    set bundle_js_filename "bundle_${bundle_md5}.js"
    set bundle_css_filename "bundle_${bundle_md5}.css"
    set config_filepath [file normalize [file join [::thtml::get_cachedir] rollup.config.cjs]]

    writeFile $config_filepath [subst -nocommands -nobackslashes {
        module.exports = {
            input: '${entryfile}',
            output: {
                dir: '${rollup_outdir}',
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
                    output: '${bundle_css_filename}',
                    alwaysOutput: true,
                    minify: true,
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

    if { [catch {set bundle_js [exec -ignorestderr -- npx --no-install rollup -c ${config_filepath}]} errmsg] } {
        file delete $config_filepath
        error "rollup error: $errmsg"
    }
    file delete $config_filepath
    #return $bundle_js
}