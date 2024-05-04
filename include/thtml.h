#ifndef THTML_H
#define THTML_H

#include <tcl.h>
#include <string.h>

int thtml_streq(Tcl_Obj *a, Tcl_Obj *b) {
    Tcl_Size a_len;
    const char *a_str = Tcl_GetStringFromObj(a, &a_len);

    Tcl_Size b_len;
    const char *b_str = Tcl_GetStringFromObj(b, &b_len);

    if (a_len != b_len) {
        return 0;
    }

    return memcmp(a_str, b_str, a_len) == 0;
}

int thtml_strneq(Tcl_Obj *a, Tcl_Obj *b) {
    return !thtml_streq(a, b);
}

int _thtml_gt(Tcl_Obj *a, Tcl_Obj *b) {

    // todo: check if string is empty

    double a_val, b_val;
    if (Tcl_GetDoubleFromObj(NULL, a, &a_val) != TCL_OK) {
        return 0;
    }
    if (Tcl_GetDoubleFromObj(NULL, b, &b_val) != TCL_OK) {
        return 0;
    }
    return a_val > b_val;
}

#endif // THTML_H