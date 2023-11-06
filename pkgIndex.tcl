set dir [file dirname [info script]]

package ifneeded thtml 1.0.0 [list source [file join $dir tcl thtml.tcl]]
