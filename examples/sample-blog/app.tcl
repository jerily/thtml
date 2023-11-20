package require thtml

::thtml::init [dict create \
    cache 0 \
    rootdir [file dirname [info script]]]

puts [::thtml::renderfile index.thtml {title "My Page" content "Hello World!"}]
