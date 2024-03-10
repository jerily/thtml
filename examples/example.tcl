package require thtml

::thtml::init {cache 1}

set template {<html><body><h1>${title}</h1></body></html>}
puts first_invocation=[::thtml::render $template {title "hello world"}]
puts second_invocation=[::thtml::render $template {title "this is a test"}]
