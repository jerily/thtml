#ifndef THTML_H
#define THTML_H

#include <tcl.h>
#include <string.h>

#ifndef TCL_SIZE_MAX
typedef int Tcl_Size;
# define Tcl_GetSizeIntFromObj Tcl_GetIntFromObj
# define Tcl_NewSizeIntObj Tcl_NewIntObj
# define TCL_SIZE_MAX      INT_MAX
# define TCL_SIZE_MODIFIER ""
#endif

int __thtml_streq__(Tcl_Obj *a, Tcl_Obj *b) {
    Tcl_Size a_len;
    const char *a_str = Tcl_GetStringFromObj(a, &a_len);

    Tcl_Size b_len;
    const char *b_str = Tcl_GetStringFromObj(b, &b_len);

    if (a_len != b_len) {
        return 0;
    }

    return memcmp(a_str, b_str, a_len) == 0;
}

int __thtml_strneq__(Tcl_Obj *a, Tcl_Obj *b) {
    return !__thtml_streq__(a, b);
}

int __thtml_gt__(Tcl_Obj *a, Tcl_Obj *b) {

    // todo: check if string is empty

    void *a_val = NULL;
    void *b_val = NULL;
    int a_type = TCL_NUMBER_NAN;
    int b_type = TCL_NUMBER_NAN;
    if (TCL_OK != Tcl_GetNumberFromObj(NULL, a, &a_val, &a_type) || TCL_OK != Tcl_GetNumberFromObj(NULL, b, &b_val, &b_type)) {
        fprintf(stderr, "error: a=%s b=%s\n", Tcl_GetString(a), Tcl_GetString(b));
        return 0;
    }

    if (a_type == TCL_NUMBER_NAN || b_type == TCL_NUMBER_NAN) {
        return 0;
    }

    if (a_type == TCL_NUMBER_DOUBLE || b_type == TCL_NUMBER_DOUBLE) {
        return *(double *)a_val > *(double *)b_val;
    }

    if (a_type == TCL_NUMBER_INT || b_type == TCL_NUMBER_INT) {
        return *(int *)a_val > *(int *)b_val;
    }

    return 0;
}

double __thtml_add__(Tcl_Obj *a, Tcl_Obj *b) {

    // todo: check if string is empty

    double a_val, b_val;
    if (Tcl_GetDoubleFromObj(NULL, a, &a_val) != TCL_OK) {
        return 0;
    }
    if (Tcl_GetDoubleFromObj(NULL, b, &b_val) != TCL_OK) {
        return 0;
    }
    return a_val + b_val;
}

#endif // THTML_H