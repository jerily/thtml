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

// escape "<", ">", "&" characters when inside html tag attributes (quotes) or commands (square brackets)
void thtml_EscapeTemplate(const char *p, const char *end, Tcl_DString *dsPtr) {
    int inside_otag = 0;
    int inside_doublequote_attvalue = 0;
    int inside_singlequote_attvalue = 0;
    int open_square_brackets = 0;
    int escaped = 0;
    while (p < end) {
        if ((inside_otag && inside_doublequote_attvalue && *p != '\"') || (inside_otag && inside_singlequote_attvalue && *p != '\'') || open_square_brackets > 0) {
            if (*p == '&') {
                Tcl_DStringAppend(dsPtr, "&amp;", 5);
            } else if (*p == '<') {
                Tcl_DStringAppend(dsPtr, "&lt;", 4);
            } else if (*p == '>') {
                Tcl_DStringAppend(dsPtr, "&gt;", 4);
            } else {
                if (*p == '[') {
                    open_square_brackets++;
                } else if (*p == ']') {
                    open_square_brackets--;
                }
                Tcl_DStringAppend(dsPtr, p, 1);
            }
        } else if (inside_otag) {
            if (*p == '"') {
                if (!escaped) { inside_doublequote_attvalue = !inside_doublequote_attvalue; }
            } else if (*p == '\'') {
                if (!escaped) { inside_singlequote_attvalue = !inside_singlequote_attvalue; }
            } else if (*p == '>') {
                inside_otag = 0;
            }
            Tcl_DStringAppend(dsPtr, p, 1);
        } else if (*p == '<') {
            inside_otag = 1;
            Tcl_DStringAppend(dsPtr, p, 1);
        } else if (*p == '[') {
            open_square_brackets++;
            Tcl_DStringAppend(dsPtr, p, 1);
        } else if (*p == ']') {
            open_square_brackets--;
            Tcl_DStringAppend(dsPtr, p, 1);
        } else if (*p == '\\') {
            escaped = !escaped;
            Tcl_DStringAppend(dsPtr, p, 1);
            p++;
            continue;
        } else {
            Tcl_DStringAppend(dsPtr, p, 1);
        }
        escaped = 0;
        p++;
    }
}