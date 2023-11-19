# thtml

## Prerequisites

* [tdom](http://www.tdom.org/) - Tcl XML parser

## Examples

### foreach

Template:
```html
set template {
    <tpl foreach="item" in="${items}">
        <div class="item">
            <span class="name">${item.name}</span>
            <span class="value">${item.value}</span>
        </div>
    </tpl>
}
```

TCL:
```tcl
::thtml::render $template {
    item {
        name "item1"
        value "value1"
    }
    item {
        name "item2"
        value "value2"
    }
}
```

Expected output:
```html
<div class="item">
    <span class="name">item1</span>
    <span class="value">value1</span>
</div>
<div class="item">
    <span class="name">item2</span>
    <span class="value">value2</span>
</div>
```

### if

Template:
```html
set template {
    <tpl if='${value} eq "one"'>
        <div class="value1">Value is 1</div>
    </tpl>
    <tpl if='${value} eq "two"'>
        <div class="value2">Value is 2</div>
    </tpl>
    <tpl if='${value} eq "three"'>
        <div class="value3">Value is 3</div>
    </tpl>
}
```

TCL:
```tcl
::thtml::render $template {value one}
```

Expected output:
```html
<div class="value1">Value is 1</div>
```

### include

Template:
```html
set template {
    <tpl include="header.thtml" title="You are awesome!" />
    <div class="content">Content</div>
    <tpl include="footer.thtml" />
}
```

TCL:
```tcl
::thtml::render $template {}
```

Expected output:
```html
    <h1>You are awesome!</h1>
    <div class="content">Content</div>
    <strong>Footer</strong>
```

Where ```header.thtml``` is:
```html
<h1>@{title}</h1>
```

And ```footer.thtml``` is:
```html
<strong>Footer</strong>
```

### val

Template:
```html
set template {
    <html>
        <tpl val="x">return 1</tpl>
        <tpl val="y">return 2</tpl>
        <tpl val="z">return [expr { $x + $y }]</tpl>
        <head>
            <title>${title}</title>
        </head>
        <body>
            <h1>${title}</h1>
            <p>${z}</p>
        </body>
    </html>
}
```

TCL:
```tcl
::thtml::render $template {title "Hello World!"}
```