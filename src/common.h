/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#ifndef THTML_COMMON_H
#define THTML_COMMON_H

#include <tcl.h>

#ifndef TCL_SIZE_MAX
typedef int Tcl_Size;
# define Tcl_GetSizeIntFromObj Tcl_GetIntFromObj
# define Tcl_NewSizeIntObj Tcl_NewIntObj
# define TCL_SIZE_MAX      INT_MAX
# define TCL_SIZE_MODIFIER ""
#endif

#define XSTR(s) STR(s)
#define STR(s) #s

#define UNUSED(expr) do { (void)(expr); } while (0)

#ifdef DEBUG
# define DBG(x) x
#ifndef __FUNCTION_NAME__
#ifdef _WIN32   // WINDOWS
#define __FUNCTION_NAME__   __FUNCTION__
#else          // GCC
#define __FUNCTION_NAME__   __func__
#endif
#endif
# define DBG2(x) {printf("%s: ", __FUNCTION_NAME__); x; printf("\n"); fflush(stdout);}
#else
# define DBG(x)
# define DBG2(x)
#endif

#define ObjCmdProc(x) int (x)(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])

#define CheckArgs(min, max, n, msg) \
                 if ((objc < min) || (objc >max)) { \
                     Tcl_WrongNumArgs(interp, n, objv, msg); \
                     return TCL_ERROR; \
                 }

#define SetResult(str) Tcl_ResetResult(interp); \
                     Tcl_SetStringObj(Tcl_GetObjResult(interp), (str), -1)

#define CHARTYPE(what, c) (is ## what ((int)((unsigned char)(c))))

void thtml_AppendEscaped(const char *p, const char *end, Tcl_DString *dsPtr);
void thtml_EscapeTemplate(const char *p, const char *end, Tcl_DString *dsPtr);


#endif //THTML_COMMON_H
