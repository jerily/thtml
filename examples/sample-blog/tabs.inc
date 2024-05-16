<tpl val="tabs">
    return {{url / title Home} {url /blog title Blog}}
</tpl>
<tpl val="a b c d e">
    return "1 345"
</tpl>
<tpl val="a b c d f">
    return "heading: $heading - this is a test [expr {$heading eq {hello world}}] - [string index ${a.b.c.d.e} 2]"
</tpl>
<tpl val="first_tab">
    lindex $tabs 0
</tpl>
<div>
    <h2>$heading [lindex $tabs 0]</h2>

    <tpl foreach="tab" in="$tabs" indexvar="i">
        <tpl if="$i > 0"> | </tpl>
        <tpl if="$path eq ${tab.url}">
            <strong>${tab.title}</strong>
        </tpl>
        <tpl if="$path ne ${tab.url}">
            <a href="${tab.url}">${tab.title}</a>
        </tpl>
    </tpl>
    <tpl if="$path eq {hello world} || $path eq {blah blah}">
        <p>Hello World!</p>
    </tpl>

    abcdf = ${a.b.c.d.f}
</div>