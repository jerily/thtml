#include "common.h"

void thtml_AppendEscaped(const char *p, const char *end, Tcl_DString *dsPtr) {
    while (p < end) {
        if (*p == '\n') {
            Tcl_DStringAppend(dsPtr, "\\n", 2);
        } else if (*p == '\r') {
            Tcl_DStringAppend(dsPtr, "\\r", 2);
//        } else if (*p == '"') {
//            Tcl_DStringAppend(dsPtr, "\\\"", 2);
//        } else if (*p == '\\') {
//            Tcl_DStringAppend(dsPtr, "\\\\", 2);
        } else {
            Tcl_DStringAppend(dsPtr, p, 1);
        }
        p++;
    }
}
