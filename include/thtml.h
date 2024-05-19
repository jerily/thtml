#ifndef THTML_H
#define THTML_H

#include <tcl.h>
#include <string.h>
#include <assert.h>

#ifndef TCL_SIZE_MAX
typedef int Tcl_Size;
# define Tcl_GetSizeIntFromObj Tcl_GetIntFromObj
# define Tcl_NewSizeIntObj Tcl_NewIntObj
# define TCL_SIZE_MAX      INT_MAX
# define TCL_SIZE_MODIFIER ""
#endif

int __thtml_string_compare__(Tcl_Obj *a, Tcl_Obj *b) {
    Tcl_Size a_len;
    const char *a_str = Tcl_GetStringFromObj(a, &a_len);

    Tcl_Size b_len;
    const char *b_str = Tcl_GetStringFromObj(b, &b_len);

    if (a_len != b_len) {
        return a_len - b_len;
    }

    return memcmp(a_str, b_str, a_len);
}

Tcl_Obj *__thtml_streq__(Tcl_Obj *a, Tcl_Obj *b) {
    return Tcl_NewBooleanObj(__thtml_string_compare__(a, b) == 0);
}

Tcl_Obj *__thtml_strneq__(Tcl_Obj *a, Tcl_Obj *b) {
    return Tcl_NewBooleanObj(__thtml_string_compare__(a, b) != 0);
}

int __thtml_compare_op__(Tcl_Obj *a, Tcl_Obj *b) {

    // todo: check if string is empty

    void *a_val = NULL;
    void *b_val = NULL;
    int a_type = TCL_NUMBER_NAN;
    int b_type = TCL_NUMBER_NAN;
    if (TCL_OK != Tcl_GetNumberFromObj(NULL, a, &a_val, &a_type) ||
        TCL_OK != Tcl_GetNumberFromObj(NULL, b, &b_val, &b_type) || a_type == TCL_NUMBER_NAN ||
        b_type == TCL_NUMBER_NAN) {
        // we should never get here
        // we check before calling this function
        fprintf(stderr, "error: a=%s b=%s\n", Tcl_GetString(a), Tcl_GetString(b));
        assert(0);
        return 0;
    }

    if (a_type == TCL_NUMBER_DOUBLE || b_type == TCL_NUMBER_DOUBLE) {
        double res = *(const double *)a_val - *(const double *)b_val;
        return res < 0 ? -1 : (res > 0 ? 1 : 0);
    } else if (a_type == TCL_NUMBER_BIG || b_type == TCL_NUMBER_BIG) {
        Tcl_WideInt res = *(const Tcl_WideInt *)a_val - *(const Tcl_WideInt *)b_val;
        return res < 0 ? -1 : (res > 0 ? 1 : 0);
    } else if (a_type == TCL_NUMBER_INT || b_type == TCL_NUMBER_INT) {
        int res = *(const int *)a_val - *(const int *)b_val;
        return res < 0 ? -1 : (res > 0 ? 1 : 0);
    }

    // we should never get here
    // we check before calling this function
    assert(0);
    return 0;
}

Tcl_Obj *__thtml_gt__(Tcl_Obj *a, Tcl_Obj *b) {
    return Tcl_NewBooleanObj(__thtml_compare_op__(a, b) > 0);
}

Tcl_Obj *__thtml_gte__(Tcl_Obj *a, Tcl_Obj *b) {
    return Tcl_NewBooleanObj(__thtml_compare_op__(a, b) >= 0);
}

Tcl_Obj *__thtml_lt__(Tcl_Obj *a, Tcl_Obj *b) {
    return Tcl_NewBooleanObj(__thtml_compare_op__(a, b) < 0);
}

Tcl_Obj *__thtml_lte__(Tcl_Obj *a, Tcl_Obj *b) {
    return Tcl_NewBooleanObj(__thtml_compare_op__(a, b) <= 0);
}

Tcl_Obj *__thtml_eq__(Tcl_Obj *a, Tcl_Obj *b) {
    return Tcl_NewBooleanObj(__thtml_compare_op__(a, b) == 0);
}

Tcl_Obj *__thtml_ne__(Tcl_Obj *a, Tcl_Obj *b) {
    return Tcl_NewBooleanObj(__thtml_compare_op__(a, b) != 0);
}

Tcl_Obj *__thtml_and__(Tcl_Obj *a, Tcl_Obj *b) {
    int a_val, b_val;
    if (Tcl_GetBooleanFromObj(NULL, a, &a_val) != TCL_OK) {
        return 0;
    }
    if (Tcl_GetBooleanFromObj(NULL, b, &b_val) != TCL_OK) {
        return 0;
    }
    return Tcl_NewBooleanObj(a_val && b_val);
}

Tcl_Obj *__thtml_or__(Tcl_Obj *a, Tcl_Obj *b) {
    int a_val, b_val;
    if (Tcl_GetBooleanFromObj(NULL, a, &a_val) != TCL_OK) {
        return 0;
    }
    if (Tcl_GetBooleanFromObj(NULL, b, &b_val) != TCL_OK) {
        return 0;
    }
    return Tcl_NewBooleanObj(a_val || b_val);
}

Tcl_Obj *__thtml_not__(Tcl_Obj *a) {
    int a_val;
    if (Tcl_GetBooleanFromObj(NULL, a, &a_val) != TCL_OK) {
        return 0;
    }
    return Tcl_NewBooleanObj(!a_val);
}

Tcl_Obj *__thtml_add__(Tcl_Obj *a, Tcl_Obj *b) {
    void *a_val = NULL;
    void *b_val = NULL;
    int a_type = TCL_NUMBER_NAN;
    int b_type = TCL_NUMBER_NAN;
    if (TCL_OK != Tcl_GetNumberFromObj(NULL, a, &a_val, &a_type) ||
        TCL_OK != Tcl_GetNumberFromObj(NULL, b, &b_val, &b_type) || a_type == TCL_NUMBER_NAN ||
        b_type == TCL_NUMBER_NAN) {
        // we should never get here
        // we check before calling this function
        fprintf(stderr, "error: a=%s b=%s\n", Tcl_GetString(a), Tcl_GetString(b));
        assert(0);
        return 0;
    }

    if (a_type == TCL_NUMBER_DOUBLE || b_type == TCL_NUMBER_DOUBLE) {
        return Tcl_NewDoubleObj(*(const double *)a_val + *(const double *)b_val);
    } else if (a_type == TCL_NUMBER_BIG || b_type == TCL_NUMBER_BIG) {
        return Tcl_NewWideIntObj(*(const Tcl_WideInt *)a_val + *(const Tcl_WideInt *)b_val);
    } else if (a_type == TCL_NUMBER_INT || b_type == TCL_NUMBER_INT) {
        return Tcl_NewIntObj(*(const int *)a_val + *(const int *)b_val);
    }

    assert(0);
    return NULL;
}

Tcl_Obj *__thtml_sub__(Tcl_Obj *a, Tcl_Obj *b) {
    void *a_val = NULL;
    void *b_val = NULL;
    int a_type = TCL_NUMBER_NAN;
    int b_type = TCL_NUMBER_NAN;
    if (TCL_OK != Tcl_GetNumberFromObj(NULL, a, &a_val, &a_type) ||
        TCL_OK != Tcl_GetNumberFromObj(NULL, b, &b_val, &b_type) || a_type == TCL_NUMBER_NAN ||
        b_type == TCL_NUMBER_NAN) {
        // we should never get here
        // we check before calling this function
        fprintf(stderr, "error: a=%s b=%s\n", Tcl_GetString(a), Tcl_GetString(b));
        assert(0);
        return 0;
    }

    if (a_type == TCL_NUMBER_DOUBLE || b_type == TCL_NUMBER_DOUBLE) {
        return Tcl_NewDoubleObj(*(const double *)a_val - *(const double *)b_val);
    } else if (a_type == TCL_NUMBER_BIG || b_type == TCL_NUMBER_BIG) {
        return Tcl_NewWideIntObj(*(const Tcl_WideInt *)a_val - *(const Tcl_WideInt *)b_val);
    } else if (a_type == TCL_NUMBER_INT || b_type == TCL_NUMBER_INT) {
        return Tcl_NewIntObj(*(const int *)a_val - *(const int *)b_val);
    }

    assert(0);
    return NULL;
}

Tcl_Obj *__thtml_mult__(Tcl_Obj *a, Tcl_Obj *b) {
    void *a_val = NULL;
    void *b_val = NULL;
    int a_type = TCL_NUMBER_NAN;
    int b_type = TCL_NUMBER_NAN;
    if (TCL_OK != Tcl_GetNumberFromObj(NULL, a, &a_val, &a_type) ||
        TCL_OK != Tcl_GetNumberFromObj(NULL, b, &b_val, &b_type) || a_type == TCL_NUMBER_NAN ||
        b_type == TCL_NUMBER_NAN) {
        // we should never get here
        // we check before calling this function
        fprintf(stderr, "error: a=%s b=%s\n", Tcl_GetString(a), Tcl_GetString(b));
        assert(0);
        return 0;
    }

    if (a_type == TCL_NUMBER_DOUBLE || b_type == TCL_NUMBER_DOUBLE) {
        return Tcl_NewDoubleObj(*(const double *)a_val * *(const double *)b_val);
    } else if (a_type == TCL_NUMBER_BIG || b_type == TCL_NUMBER_BIG) {
        return Tcl_NewWideIntObj(*(const Tcl_WideInt *)a_val * *(const Tcl_WideInt *)b_val);
    } else if (a_type == TCL_NUMBER_INT || b_type == TCL_NUMBER_INT) {
        return Tcl_NewIntObj(*(const int *)a_val * *(const int *)b_val);
    }

    assert(0);
    return NULL;
}

Tcl_Obj *__thtml_div__(Tcl_Obj *a, Tcl_Obj *b) {
    void *a_val = NULL;
    void *b_val = NULL;
    int a_type = TCL_NUMBER_NAN;
    int b_type = TCL_NUMBER_NAN;
    if (TCL_OK != Tcl_GetNumberFromObj(NULL, a, &a_val, &a_type) ||
        TCL_OK != Tcl_GetNumberFromObj(NULL, b, &b_val, &b_type) || a_type == TCL_NUMBER_NAN ||
        b_type == TCL_NUMBER_NAN) {
        // we should never get here
        // we check before calling this function
        fprintf(stderr, "error: a=%s b=%s\n", Tcl_GetString(a), Tcl_GetString(b));
        assert(0);
        return 0;
    }

    if (a_type == TCL_NUMBER_DOUBLE || b_type == TCL_NUMBER_DOUBLE) {
        return Tcl_NewDoubleObj(*(const double *)a_val / *(const double *)b_val);
    } else if (a_type == TCL_NUMBER_BIG || b_type == TCL_NUMBER_BIG) {
        return Tcl_NewWideIntObj(*(const Tcl_WideInt *)a_val / *(const Tcl_WideInt *)b_val);
    } else if (a_type == TCL_NUMBER_INT || b_type == TCL_NUMBER_INT) {
        return Tcl_NewIntObj(*(const int *)a_val / *(const int *)b_val);
    }

    assert(0);
    return NULL;
}

#endif // THTML_H