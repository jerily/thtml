# thtml

HTML Templating Engine for TCL and [twebserver](https://github.com/jerily/twebserver).

**Note that this project is under active development.** 

## Prerequisites

* [tcl](https://www.tcl.tk/) (version 9.0) - TCL
* [tdom](http://www.tdom.org/) (version [0.9.4](http://tdom.org/downloads/tdom-0.9.4-src.tar.gz)) - Tcl XML parser

* If you are planning to use ```import_node_module``` and ```js``` tags, you will need:
    * [npm](https://www.npmjs.com/) - Node.js package manager
    * In your project directory, run:
    ```bash
      npm i rollup \
        @rollup/plugin-node-resolve \
        @rollup/plugin-commonjs \
        @rollup/plugin-babel \
        rollup-plugin-postcss \
        rollup-plugin-terser \
        cssnano
    ```

## Installation

```bash
git clone https://github.com/jerily/thtml.git
cd thtml
mkdir build
cd build
cmake ..
# or if TCL is not in the default path (/usr/local/lib):
# change "TCL_LIBRARY_DIR" and "TCL_INCLUDE_DIR" to the correct paths
# cmake .. -DTCL_LIBRARY_DIR=/usr/local/lib -DTCL_INCLUDE_DIR=/usr/local/include
make
make install
```

## Example

See [sample-blog](examples/sample-blog/) for a complete example.

## Supported Features

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
    items {{
        name "item1"
        value "value1"
    }
    {
        name "item2"
        value "value2"
    }}
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
    <tpl if='$value eq "one"'>
        <div class="value1">Value is 1</div>
    </tpl>
    <tpl if='$value eq "two"'>
        <div class="value2">Value is 2</div>
    </tpl>
    <tpl if='$value eq "three"'>
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
<h1>$title</h1>
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
            <title>$title</title>
        </head>
        <body>
            <h1>$title</h1>
            <p>$z</p>
        </body>
    </html>
}
```

TCL:
```tcl
::thtml::render $template {title "Hello World!"}
```