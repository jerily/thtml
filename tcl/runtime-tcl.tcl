# Copyright Jerily LTD. All Rights Reserved.
# SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
# SPDX-License-Identifier: MIT.

namespace eval ::thtml::runtime::tcl {}

proc ::thtml::runtime::tcl::evaluate_script {script} {
    return [uplevel 1 $script]
}

