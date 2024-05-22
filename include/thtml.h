/*
This software is copyrighted by the Regents of the University of
California, Sun Microsystems, Inc., Scriptics Corporation, ActiveState
Corporation and other parties. The following terms apply to this file.

The authors hereby grant permission to use, copy, modify, distribute,
and license this software and its documentation for any purpose, provided
that existing copyright notices are retained in all copies and that this
notice is included verbatim in any distributions. No written agreement,
license, or royalty fee is required for any of the authorized uses.
Modifications to this software may be copyrighted by their authors
and need not follow the licensing terms described here, provided that
the new terms are clearly indicated on the first page of each file where
they apply.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
MODIFICATIONS.

GOVERNMENT USE: If you are acquiring this software on behalf of the
U.S. government, the Government shall have only "Restricted Rights"
in the software and related documentation as defined in the Federal
Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
are acquiring the software on behalf of the Department of Defense, the
software shall be classified as "Commercial Computer Software" and the
Government shall have only "Restricted Rights" as defined in Clause
252.227-7014 (b) (3) of DFARs.  Notwithstanding the foregoing, the
authors grant the U.S. Government and others acting in its behalf
permission to use and distribute the software in accordance with the
terms specified in this license.
 */

#ifndef THTML_H
#define THTML_H

#include <tcl.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <float.h>

#ifndef TCL_SIZE_MAX
typedef int Tcl_Size;
# define Tcl_GetSizeIntFromObj Tcl_GetIntFromObj
# define Tcl_NewSizeIntObj Tcl_NewIntObj
# define TCL_SIZE_MAX      INT_MAX
# define TCL_SIZE_MODIFIER ""
#endif

#define UWIDE_MAX ((Tcl_WideUInt)-1)
#define WIDE_MAX ((Tcl_WideInt)(UWIDE_MAX >> 1))
#define WIDE_MIN ((Tcl_WideInt)((Tcl_WideUInt)WIDE_MAX+1))

#define DIVIDED_BY_ZERO        ((Tcl_Obj *) -1)
#define EXPONENT_OF_ZERO    ((Tcl_Obj *) -2)
#define GENERAL_ARITHMETIC_ERROR ((Tcl_Obj *) -3)
#define OUT_OF_MEMORY ((Tcl_Obj *) -4)

enum MathInstruction {
    INST_BITOR,
    INST_BITXOR,
    INST_BITAND,
    INST_EQ,
    INST_NEQ,
    INST_LT,
    INST_GT,
    INST_LE,
    INST_GE,
    INST_LSHIFT,
    INST_RSHIFT,
    INST_ADD,
    INST_SUB,
    INST_MULT,
    INST_DIV,
    INST_MOD,
    INST_UPLUS,
    INST_UMINUS,
    INST_BITNOT,
    INST_LNOT,
    INST_EXPON
};

/*
 * Macro used to make the check for type overflow more mnemonic. This works by
 * comparing sign bits; the rest of the word is irrelevant. The ANSI C
 * "prototype" (where inttype_t is any integer type) is:
 *
 * MODULE_SCOPE int Overflowing(inttype_t a, inttype_t b, inttype_t sum);
 *
 * Check first the condition most likely to fail in usual code (at least for
 * usage in [incr]: do the first summand and the sum have != signs?
 */

#define Overflowing(a,b,sum) ((((a)^(sum)) < 0) && (((a)^(b)) >= 0))
/*
 * Auxiliary tables used to compute powers of small integers.
 */

/*
 * Maximum base that, when raised to powers 2, 3, ..., 16, fits in a
 * Tcl_WideInt.
 */

static const Tcl_WideInt MaxBase64[] = {
        (Tcl_WideInt) 46340 * 65536 + 62259,    /* 3037000499 == isqrt(2**63-1) */
        (Tcl_WideInt) 2097151, (Tcl_WideInt) 55108, (Tcl_WideInt) 6208,
        (Tcl_WideInt) 1448, (Tcl_WideInt) 511, (Tcl_WideInt) 234, (Tcl_WideInt) 127,
        (Tcl_WideInt) 78, (Tcl_WideInt) 52, (Tcl_WideInt) 38, (Tcl_WideInt) 28,
        (Tcl_WideInt) 22, (Tcl_WideInt) 18, (Tcl_WideInt) 15
};
static const size_t MaxBase64Size = sizeof(MaxBase64) / sizeof(Tcl_WideInt);

/*
 * Table giving 3, 4, ..., 13 raised to powers greater than 16 when the
 * results fit in a 64-bit signed integer.
 */

static const unsigned short Exp64Index[] = {
        0, 23, 38, 49, 57, 63, 67, 70, 72, 74, 75, 76
};
static const size_t Exp64IndexSize =
        sizeof(Exp64Index) / sizeof(unsigned short);
static const Tcl_WideInt Exp64Value[] = {
        (Tcl_WideInt) 243 * 243 * 243 * 3 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 3 * 3 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 3 * 3 * 3 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 3 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 3 * 3 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 3 * 3 * 3 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243 * 3 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243 * 3 * 3 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243 * 3 * 3 * 3 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243 * 243,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243 * 243 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243 * 243 * 3 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243 * 243 * 3 * 3 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243 * 243 * 3 * 3 * 3 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243 * 243 * 243,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243 * 243 * 243 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243 * 243 * 243 * 3 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243 * 243 * 243 * 3 * 3 * 3,
        (Tcl_WideInt) 243 * 243 * 243 * 243 * 243 * 243 * 243 * 3 * 3 * 3 * 3,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 4 * 4,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 4 * 4 * 4,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 4 * 4 * 4 * 4,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 1024,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 1024 * 4,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 1024 * 4 * 4,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 1024 * 4 * 4 * 4,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 1024 * 4 * 4 * 4 * 4,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 1024 * 1024,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 1024 * 1024 * 4,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 1024 * 1024 * 4 * 4,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 1024 * 1024 * 4 * 4 * 4,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 1024 * 1024 * 4 * 4 * 4 * 4,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 1024 * 1024 * 1024,
        (Tcl_WideInt) 1024 * 1024 * 1024 * 1024 * 1024 * 1024 * 4,
        (Tcl_WideInt) 3125 * 3125 * 3125 * 5 * 5,
        (Tcl_WideInt) 3125 * 3125 * 3125 * 5 * 5 * 5,
        (Tcl_WideInt) 3125 * 3125 * 3125 * 5 * 5 * 5 * 5,
        (Tcl_WideInt) 3125 * 3125 * 3125 * 3125,
        (Tcl_WideInt) 3125 * 3125 * 3125 * 3125 * 5,
        (Tcl_WideInt) 3125 * 3125 * 3125 * 3125 * 5 * 5,
        (Tcl_WideInt) 3125 * 3125 * 3125 * 3125 * 5 * 5 * 5,
        (Tcl_WideInt) 3125 * 3125 * 3125 * 3125 * 5 * 5 * 5 * 5,
        (Tcl_WideInt) 3125 * 3125 * 3125 * 3125 * 3125,
        (Tcl_WideInt) 3125 * 3125 * 3125 * 3125 * 3125 * 5,
        (Tcl_WideInt) 3125 * 3125 * 3125 * 3125 * 3125 * 5 * 5,
        (Tcl_WideInt) 7776 * 7776 * 7776 * 6 * 6,
        (Tcl_WideInt) 7776 * 7776 * 7776 * 6 * 6 * 6,
        (Tcl_WideInt) 7776 * 7776 * 7776 * 6 * 6 * 6 * 6,
        (Tcl_WideInt) 7776 * 7776 * 7776 * 7776,
        (Tcl_WideInt) 7776 * 7776 * 7776 * 7776 * 6,
        (Tcl_WideInt) 7776 * 7776 * 7776 * 7776 * 6 * 6,
        (Tcl_WideInt) 7776 * 7776 * 7776 * 7776 * 6 * 6 * 6,
        (Tcl_WideInt) 7776 * 7776 * 7776 * 7776 * 6 * 6 * 6 * 6,
        (Tcl_WideInt) 16807 * 16807 * 16807 * 7 * 7,
        (Tcl_WideInt) 16807 * 16807 * 16807 * 7 * 7 * 7,
        (Tcl_WideInt) 16807 * 16807 * 16807 * 7 * 7 * 7 * 7,
        (Tcl_WideInt) 16807 * 16807 * 16807 * 16807,
        (Tcl_WideInt) 16807 * 16807 * 16807 * 16807 * 7,
        (Tcl_WideInt) 16807 * 16807 * 16807 * 16807 * 7 * 7,
        (Tcl_WideInt) 32768 * 32768 * 32768 * 8 * 8,
        (Tcl_WideInt) 32768 * 32768 * 32768 * 8 * 8 * 8,
        (Tcl_WideInt) 32768 * 32768 * 32768 * 8 * 8 * 8 * 8,
        (Tcl_WideInt) 32768 * 32768 * 32768 * 32768,
        (Tcl_WideInt) 59049 * 59049 * 59049 * 9 * 9,
        (Tcl_WideInt) 59049 * 59049 * 59049 * 9 * 9 * 9,
        (Tcl_WideInt) 59049 * 59049 * 59049 * 9 * 9 * 9 * 9,
        (Tcl_WideInt) 100000 * 100000 * 100000 * 10 * 10,
        (Tcl_WideInt) 100000 * 100000 * 100000 * 10 * 10 * 10,
        (Tcl_WideInt) 161051 * 161051 * 161051 * 11 * 11,
        (Tcl_WideInt) 161051 * 161051 * 161051 * 11 * 11 * 11,
        (Tcl_WideInt) 248832 * 248832 * 248832 * 12 * 12,
        (Tcl_WideInt) 371293 * 371293 * 371293 * 13 * 13
};
static const size_t Exp64ValueSize = sizeof(Exp64Value) / sizeof(Tcl_WideInt);


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

/*
 *----------------------------------------------------------------------
 *
 * CompareTwoNumbers --
 *
 *	This function compares a pair of numbers in Tcl_Objs. Each argument
 *	must already be known to be numeric and not NaN.
 *
 * Results:
 *	One of MP_LT, MP_EQ or MP_GT, depending on whether valuePtr is less
 *	than, equal to, or greater than value2Ptr (respectively).
 *
 * Side effects:
 *	None, provided both values are numeric.
 *
 *----------------------------------------------------------------------
 */

int
__thtml_compare_two_numbers__(
        Tcl_Obj *a,
        Tcl_Obj *b)
{
    int type1 = TCL_NUMBER_NAN, type2 = TCL_NUMBER_NAN, compare;
    void *ptr1, *ptr2;
    double d1, d2, tmp;
    Tcl_WideInt w1, w2;

    (void) Tcl_GetNumberFromObj(NULL, a, &ptr1, &type1);
    (void) Tcl_GetNumberFromObj(NULL, b, &ptr2, &type2);

    switch (type1) {
        case TCL_NUMBER_INT:
            w1 = *((const Tcl_WideInt *)ptr1);
            switch (type2) {
                case TCL_NUMBER_INT:
                    w2 = *((const Tcl_WideInt *)ptr2);
                wideCompare:
                    return (w1 < w2) ? -1 : ((w1 > w2) ? 1 : 0);
                case TCL_NUMBER_DOUBLE:
                    d2 = *((const double *)ptr2);
                    d1 = (double) w1;

                    /*
                     * If the double has a fractional part, or if the Tcl_WideInt can be
                     * converted to double without loss of precision, then compare as
                     * doubles.
                     */

                    if (DBL_MANT_DIG > CHAR_BIT*sizeof(Tcl_WideInt) || w1 == (Tcl_WideInt)d1
                        || modf(d2, &tmp) != 0.0) {
                        goto doubleCompare;
                    }

                    /*
                     * Otherwise, to make comparision based on full precision, need to
                     * convert the double to a suitably sized integer.
                     *
                     * Need this to get comparsions like
                     *	  expr 20000000000000003 < 20000000000000004.0
                     * right. Converting the first argument to double will yield two
                     * double values that are equivalent within double precision.
                     * Converting the double to an integer gets done exactly, then
                     * integer comparison can tell the difference.
                     */

                    if (d2 < (double)WIDE_MIN) {
                        return 1;
                    }
                    if (d2 > (double)WIDE_MAX) {
                        return -1;
                    }
                    w2 = (Tcl_WideInt)d2;
                    goto wideCompare;
//                case TCL_NUMBER_BIG:
//                    Tcl_TakeBignumFromObj(NULL, b, &big2);
//                    if (mp_isneg(&big2)) {
//                        compare = MP_GT;
//                    } else {
//                        compare = MP_LT;
//                    }
//                    mp_clear(&big2);
//                    return compare;
            }
            break;

        case TCL_NUMBER_DOUBLE:
            d1 = *((const double *)ptr1);
            switch (type2) {
                case TCL_NUMBER_DOUBLE:
                    d2 = *((const double *)ptr2);
                doubleCompare:
                    return (d1 < d2) ? -1 : ((d1 > d2) ? 1 : 0);
                case TCL_NUMBER_INT:
                    w2 = *((const Tcl_WideInt *)ptr2);
                    d2 = (double) w2;
                    if (DBL_MANT_DIG > CHAR_BIT*sizeof(Tcl_WideInt)
                        || w2 == (Tcl_WideInt)d2 || modf(d1, &tmp) != 0.0) {
                        goto doubleCompare;
                    }
                    if (d1 < (double)WIDE_MIN) {
                        return -1;
                    }
                    if (d1 > (double)WIDE_MAX) {
                        return 1;
                    }
                    w1 = (Tcl_WideInt)d1;
                    goto wideCompare;
//                case TCL_NUMBER_BIG:
//                    if (isinf(d1)) {
//                        return (d1 > 0.0) ? 1 : -1;
//                    }
//                    Tcl_TakeBignumFromObj(NULL, b, &big2);
//                    if ((d1 < (double)WIDE_MAX) && (d1 > (double)WIDE_MIN)) {
//                        if (mp_isneg(&big2)) {
//                            compare = MP_GT;
//                        } else {
//                            compare = MP_LT;
//                        }
//                        mp_clear(&big2);
//                        return compare;
//                    }
//                    if (DBL_MANT_DIG > CHAR_BIT*sizeof(Tcl_WideInt)
//                        && modf(d1, &tmp) != 0.0) {
//                        d2 = TclBignumToDouble(&big2);
//                        mp_clear(&big2);
//                        goto doubleCompare;
//                    }
//                    Tcl_InitBignumFromDouble(NULL, d1, &big1);
//                    goto bigCompare;
            }
            break;

//        case TCL_NUMBER_BIG:
//            Tcl_TakeBignumFromObj(NULL, a, &big1);
//            switch (type2) {
//                case TCL_NUMBER_INT:
//                    compare = mp_cmp_d(&big1, 0);
//                    mp_clear(&big1);
//                    return compare;
//                case TCL_NUMBER_DOUBLE:
//                    d2 = *((const double *)ptr2);
//                    if (isinf(d2)) {
//                        compare = (d2 > 0.0) ? MP_LT : MP_GT;
//                        mp_clear(&big1);
//                        return compare;
//                    }
//                    if ((d2 < (double)WIDE_MAX) && (d2 > (double)WIDE_MIN)) {
//                        compare = mp_cmp_d(&big1, 0);
//                        mp_clear(&big1);
//                        return compare;
//                    }
//                    if (DBL_MANT_DIG > CHAR_BIT*sizeof(Tcl_WideInt)
//                        && modf(d2, &tmp) != 0.0) {
//                        d1 = TclBignumToDouble(&big1);
//                        mp_clear(&big1);
//                        goto doubleCompare;
//                    }
//                    Tcl_InitBignumFromDouble(NULL, d2, &big2);
//                    goto bigCompare;
//                case TCL_NUMBER_BIG:
//                    Tcl_TakeBignumFromObj(NULL, b, &big2);
//                bigCompare:
//                    compare = mp_cmp(&big1, &big2);
//                    mp_clear(&big1);
//                    mp_clear(&big2);
//                    return compare;
//            }
//            break;
        default:
            Tcl_Panic("unexpected number type");
    }
    return TCL_ERROR;
}

Tcl_Obj *__thtml_gt__(Tcl_Obj *a, Tcl_Obj *b) {
    return Tcl_NewBooleanObj(__thtml_compare_two_numbers__(a, b) > 0);
}

Tcl_Obj *__thtml_gte__(Tcl_Obj *a, Tcl_Obj *b) {
    return Tcl_NewBooleanObj(__thtml_compare_two_numbers__(a, b) >= 0);
}

Tcl_Obj *__thtml_lt__(Tcl_Obj *a, Tcl_Obj *b) {
    return Tcl_NewBooleanObj(__thtml_compare_two_numbers__(a, b) < 0);
}

Tcl_Obj *__thtml_lte__(Tcl_Obj *a, Tcl_Obj *b) {
    return Tcl_NewBooleanObj(__thtml_compare_two_numbers__(a, b) <= 0);
}

Tcl_Obj *__thtml_eq__(Tcl_Obj *a, Tcl_Obj *b) {
    return Tcl_NewBooleanObj(__thtml_compare_two_numbers__(a, b) == 0);
}

Tcl_Obj *__thtml_ne__(Tcl_Obj *a, Tcl_Obj *b) {
    return Tcl_NewBooleanObj(__thtml_compare_two_numbers__(a, b) != 0);
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

/*
 * WidePwrSmallExpon --
 *
 * Helper to calculate small powers of integers whose result is wide.
 */
static inline Tcl_WideInt
WidePwrSmallExpon(Tcl_WideInt w1, long exponent) {

    Tcl_WideInt wResult;

    wResult = w1 * w1;        /* b**2 */
    switch (exponent) {
        case 2:
            break;
        case 3:
            wResult *= w1;        /* b**3 */
            break;
        case 4:
            wResult *= wResult;    /* b**4 */
            break;
        case 5:
            wResult *= wResult;    /* b**4 */
            wResult *= w1;        /* b**5 */
            break;
        case 6:
            wResult *= w1;        /* b**3 */
            wResult *= wResult;    /* b**6 */
            break;
        case 7:
            wResult *= w1;        /* b**3 */
            wResult *= wResult;    /* b**6 */
            wResult *= w1;        /* b**7 */
            break;
        case 8:
            wResult *= wResult;    /* b**4 */
            wResult *= wResult;    /* b**8 */
            break;
        case 9:
            wResult *= wResult;    /* b**4 */
            wResult *= wResult;    /* b**8 */
            wResult *= w1;        /* b**9 */
            break;
        case 10:
            wResult *= wResult;    /* b**4 */
            wResult *= w1;        /* b**5 */
            wResult *= wResult;    /* b**10 */
            break;
        case 11:
            wResult *= wResult;    /* b**4 */
            wResult *= w1;        /* b**5 */
            wResult *= wResult;    /* b**10 */
            wResult *= w1;        /* b**11 */
            break;
        case 12:
            wResult *= w1;        /* b**3 */
            wResult *= wResult;    /* b**6 */
            wResult *= wResult;    /* b**12 */
            break;
        case 13:
            wResult *= w1;        /* b**3 */
            wResult *= wResult;    /* b**6 */
            wResult *= wResult;    /* b**12 */
            wResult *= w1;        /* b**13 */
            break;
        case 14:
            wResult *= w1;        /* b**3 */
            wResult *= wResult;    /* b**6 */
            wResult *= w1;        /* b**7 */
            wResult *= wResult;    /* b**14 */
            break;
        case 15:
            wResult *= w1;        /* b**3 */
            wResult *= wResult;    /* b**6 */
            wResult *= w1;        /* b**7 */
            wResult *= wResult;    /* b**14 */
            wResult *= w1;        /* b**15 */
            break;
        case 16:
            wResult *= wResult;    /* b**4 */
            wResult *= wResult;    /* b**8 */
            wResult *= wResult;    /* b**16 */
            break;
    }
    return wResult;
}


static Tcl_Obj *__thtml_binary_math_op__(Tcl_Interp *interp, int opcode, Tcl_Obj *a, Tcl_Obj *b) {
    void *a_val = NULL;
    void *b_val = NULL;
    int a_type = TCL_NUMBER_NAN;
    int b_type = TCL_NUMBER_NAN;

    (void) Tcl_GetNumberFromObj(NULL, a, &a_val, &a_type);
    (void) Tcl_GetNumberFromObj(NULL, b, &b_val, &b_type);

    double d1, d2, dResult;
    Tcl_WideInt w1, w2, wResult;
    int invalid, zero;
    int shift;

    switch (opcode) {
        case INST_MOD:
            w2 = 0;            /* silence gcc warning */
            if (b_type == TCL_NUMBER_INT) {
                w2 = *((const Tcl_WideInt *) b_val);
                if (w2 == 0) {
                    return DIVIDED_BY_ZERO;
                }
                if ((w2 == 1) || (w2 == -1)) {
                    /*
                     * Div. by |1| always yields remainder of 0.
                     */

                    return Tcl_NewWideIntObj(0);
                }
            }
            if (a_type == TCL_NUMBER_INT) {
                w1 = *((const Tcl_WideInt *) a_val);

                if (w1 == 0) {
                    /*
                     * 0 % (non-zero) always yields remainder of 0.
                     */

                    return Tcl_NewWideIntObj(0);
                }
                if (b_type == TCL_NUMBER_INT) {
                    Tcl_WideInt wQuotient, wRemainder;
                    w2 = *((const Tcl_WideInt *) b_val);
                    wQuotient = w1 / w2;

                    /*
                     * Force Tcl's integer division rules.
                     * TODO: examine for logic simplification
                     */

                    if (((wQuotient < 0)
                         || ((wQuotient == 0)
                             && ((w1 < 0 && w2 > 0)
                                 || (w1 > 0 && w2 < 0))))
                        && (wQuotient * w2 != w1)) {
                        wQuotient -= 1;
                    }
                    wRemainder = (Tcl_WideInt) ((Tcl_WideUInt) w1 -
                                                (Tcl_WideUInt) w2 * (Tcl_WideUInt) wQuotient);
                    return Tcl_NewWideIntObj(wRemainder);
                }
            }

            return GENERAL_ARITHMETIC_ERROR;

        case INST_LSHIFT:
        case INST_RSHIFT: {
            /*
             * Reject negative shift argument.
             */

            switch (b_type) {
                case TCL_NUMBER_INT:
                    invalid = (*((const Tcl_WideInt *) b_val) < 0);
                    break;
                default:
                    /* Unused, here to silence compiler warning */
                    invalid = 0;
            }
            if (invalid) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj(
                        "negative shift argument", -1));
                return GENERAL_ARITHMETIC_ERROR;
            }

            /*
             * Zero shifted any number of bits is still zero.
             */

            if ((a_type == TCL_NUMBER_INT) && (*((const Tcl_WideInt *) a_val) == 0)) {
                return Tcl_NewWideIntObj(0);
            }

            if (opcode == INST_LSHIFT) {
                /*
                 * Large left shifts create integer overflow.
                 *
                 * BEWARE! Can't use Tcl_GetIntFromObj() here because that
                 * converts values in the (unsigned) range to their signed int
                 * counterparts, leading to incorrect results.
                 */

                if ((b_type != TCL_NUMBER_INT)
                    || (*((const Tcl_WideInt *) b_val) > INT_MAX)) {
                    /*
                     * Technically, we could hold the value (1 << (INT_MAX+1)) in
                     * an mp_int, but since we're using mp_mul_2d() to do the
                     * work, and it takes only an int argument, that's a good
                     * place to draw the line.
                     */

                    Tcl_SetObjResult(interp, Tcl_NewStringObj(
                            "integer value too large to represent", -1));
                    return GENERAL_ARITHMETIC_ERROR;
                }
                shift = (int) (*((const Tcl_WideInt *) b_val));

                /*
                 * Handle shifts within the native wide range.
                 */

                if ((a_type == TCL_NUMBER_INT)
                    && ((size_t) shift < CHAR_BIT * sizeof(Tcl_WideInt))) {
                    w1 = *((const Tcl_WideInt *) a_val);
                    if (!((w1 > 0 ? w1 : ~w1)
                          & -(((Tcl_WideUInt) 1)
                            << (CHAR_BIT * sizeof(Tcl_WideInt) - 1 - shift)))) {
                        Tcl_NewWideIntObj((Tcl_WideUInt) w1 << shift);
                    }
                }
            } else {
                /*
                 * Quickly force large right shifts to 0 or -1.
                 */

                if ((b_type != TCL_NUMBER_INT)
                    || (*(const Tcl_WideInt *) b_val > INT_MAX)) {
                    /*
                     * Again, technically, the value to be shifted could be an
                     * mp_int so huge that a right shift by (INT_MAX+1) bits could
                     * not take us to the result of 0 or -1, but since we're using
                     * mp_div_2d to do the work, and it takes only an int
                     * argument, we draw the line there.
                     */

                    switch (a_type) {
                        case TCL_NUMBER_INT:
                            zero = (*(const Tcl_WideInt *) a_val > 0);
                            break;
//                        case TCL_NUMBER_BIG:
//                            Tcl_TakeBignumFromObj(NULL, valuePtr, &big1);
//                            zero = !mp_isneg(&big1);
//                            mp_clear(&big1);
//                            break;
                        default:
                            /* Unused, here to silence compiler warning. */
                            zero = 0;
                    }
                    if (zero) {
                        return Tcl_NewWideIntObj(0);
                    }
                    return Tcl_NewWideIntObj(-1);
                }
                shift = (int) (*(const Tcl_WideInt *) b_val);

                /*
                 * Handle shifts within the native wide range.
                 */

                if (a_type == TCL_NUMBER_INT) {
                    w1 = *(const Tcl_WideInt *) a_val;
                    if ((size_t) shift >= CHAR_BIT * sizeof(Tcl_WideInt)) {
                        if (w1 >= 0) {
                            return Tcl_NewWideIntObj(0);
                        }
                        return Tcl_NewWideIntObj(-1);
                    }
                    return Tcl_NewWideIntObj(w1 >> shift);
                }
            }

            return GENERAL_ARITHMETIC_ERROR;

        }
        case INST_BITOR:
        case INST_BITXOR:
        case INST_BITAND:
            if ((a_type != TCL_NUMBER_INT) || (b_type != TCL_NUMBER_INT)) {
                return GENERAL_ARITHMETIC_ERROR;
            }

            w1 = *((const Tcl_WideInt *) a_val);
            w2 = *((const Tcl_WideInt *) b_val);

            switch (opcode) {
                case INST_BITAND:
                    wResult = w1 & w2;
                    break;
                case INST_BITOR:
                    wResult = w1 | w2;
                    break;
                case INST_BITXOR:
                    wResult = w1 ^ w2;
                    break;
                default:
                    /* Unused, here to silence compiler warning. */
                    wResult = 0;
            }
            return Tcl_NewWideIntObj(wResult);

        case INST_EXPON: {
            int oddExponent = 0, negativeExponent = 0;
            unsigned short base;

            if ((a_type == TCL_NUMBER_DOUBLE) || (b_type == TCL_NUMBER_DOUBLE)) {
                Tcl_GetDoubleFromObj(NULL, a, &d1);
                Tcl_GetDoubleFromObj(NULL, b, &d2);

                if (d1 == 0.0 && d2 < 0.0) {
                    return EXPONENT_OF_ZERO;
                }
                dResult = pow(d1, d2);
                return Tcl_NewDoubleObj(dResult);
            }
            w1 = w2 = 0; /* to silence compiler warning (maybe-uninitialized) */
            if (b_type == TCL_NUMBER_INT) {
                w2 = *((const Tcl_WideInt *) b_val);
                if (w2 == 0) {
                    /*
                     * Anything to the zero power is 1.
                     */

                    return Tcl_NewWideIntObj(1);
                } else if (w2 == 1) {
                    /*
                     * Anything to the first power is itself
                     */

                    return a;
                }

                negativeExponent = (w2 < 0);
                oddExponent = (int) w2 & 1;
            } else {
                return GENERAL_ARITHMETIC_ERROR;
            }

            if (a_type == TCL_NUMBER_INT) {
                w1 = *((const Tcl_WideInt *) a_val);

                if (negativeExponent) {
                    switch (w1) {
                        case 0:
                            /*
                             * Zero to a negative power is div by zero error.
                             */

                            return EXPONENT_OF_ZERO;
                        case -1:
                            if (oddExponent) {
                                return Tcl_NewWideIntObj(-1);
                            }
                            /* fallthrough */
                        case 1:
                            /*
                             * 1 to any power is 1.
                             */

                            return Tcl_NewWideIntObj(1);
                    }
                }
            }
            if (negativeExponent) {

                /*
                 * Integers with magnitude greater than 1 raise to a negative
                 * power yield the answer zero (see TIP 123).
                 */
                return Tcl_NewWideIntObj(0);
            }

            if (a_type != TCL_NUMBER_INT) {
                return GENERAL_ARITHMETIC_ERROR;
            }

            switch (w1) {
                case 0:
                    /*
                     * Zero to a positive power is zero.
                     */

                    return Tcl_NewWideIntObj(0);
                case 1:
                    /*
                     * 1 to any power is 1.
                     */

                    return Tcl_NewWideIntObj(1);
                case -1:
                    if (!oddExponent) {
                        return Tcl_NewWideIntObj(1);
                    }
                    Tcl_NewWideIntObj(-1);
            }

            /*
             * We refuse to accept exponent arguments that exceed one mp_digit
             * which means the max exponent value is 2**28-1 = 0x0FFFFFFF =
             * 268435455, which fits into a signed 32 bit int which is within the
             * range of the Tcl_WideInt type. This means any numeric Tcl_Obj value
             * not using TCL_NUMBER_INT type must hold a value larger than we
             * accept.
             */

            if (b_type != TCL_NUMBER_INT) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj(
                        "exponent too large", -1));
                return GENERAL_ARITHMETIC_ERROR;
            }

            /* From here (up to overflowExpon) w1 and exponent w2 are wide-int's. */
            assert(a_type == TCL_NUMBER_INT && b_type == TCL_NUMBER_INT);

            if (w1 == 2) {
                /*
                 * Reduce small powers of 2 to shifts.
                 */

                if ((Tcl_WideUInt) w2 < (Tcl_WideUInt) CHAR_BIT * sizeof(Tcl_WideInt) - 1) {
                    return Tcl_NewWideIntObj(((Tcl_WideInt) 1) << (int) w2);
                }
                return GENERAL_ARITHMETIC_ERROR;
            }
            if (w1 == -2) {
                int signum = oddExponent ? -1 : 1;

                /*
                 * Reduce small powers of 2 to shifts.
                 */

                if ((Tcl_WideUInt) w2 < CHAR_BIT * sizeof(Tcl_WideInt) - 1) {
                    return Tcl_NewWideIntObj(signum * (((Tcl_WideInt) 1) << (int) w2));
                }
                return GENERAL_ARITHMETIC_ERROR;
            }
            if (w2 - 2 < (long) MaxBase64Size
                && w1 <= MaxBase64[w2 - 2]
                && w1 >= -MaxBase64[w2 - 2]) {
                /*
                 * Small powers of integers whose result is wide.
                 */
                wResult = WidePwrSmallExpon(w1, (long) w2);

                return Tcl_NewWideIntObj(wResult);
            }

            /*
             * Handle cases of powers > 16 that still fit in a 64-bit word by
             * doing table lookup.
             */

            if (w1 - 3 >= 0 && w1 - 2 < (long) Exp64IndexSize
                && w2 - 2 < (long) (Exp64ValueSize + MaxBase64Size)) {
                base = Exp64Index[w1 - 3]
                       + (unsigned short) (w2 - 2 - MaxBase64Size);
                if (base < Exp64Index[w1 - 2]) {
                    /*
                     * 64-bit number raised to intermediate power, done by
                     * table lookup.
                     */

                    return Tcl_NewWideIntObj(Exp64Value[base]);
                }
            }

            if (-w1 - 3 >= 0 && -w1 - 2 < (long) Exp64IndexSize
                && w2 - 2 < (long) (Exp64ValueSize + MaxBase64Size)) {
                base = Exp64Index[-w1 - 3]
                       + (unsigned short) (w2 - 2 - MaxBase64Size);
                if (base < Exp64Index[-w1 - 2]) {
                    /*
                     * 64-bit number raised to intermediate power, done by
                     * table lookup.
                     */

                    wResult = oddExponent ? -Exp64Value[base] : Exp64Value[base];
                    return Tcl_NewWideIntObj(wResult);
                }
            }

            return GENERAL_ARITHMETIC_ERROR;
        }
        case INST_ADD:
        case INST_SUB:
        case INST_MULT:
        case INST_DIV:
            if ((a_type == TCL_NUMBER_DOUBLE) || (b_type == TCL_NUMBER_DOUBLE)) {
                /*
                 * At least one of the values is floating-point, so perform
                 * floating point calculations.
                 */

                Tcl_GetDoubleFromObj(NULL, a, &d1);
                Tcl_GetDoubleFromObj(NULL, b, &d2);

                switch (opcode) {
                    case INST_ADD:
                        dResult = d1 + d2;
                        break;
                    case INST_SUB:
                        dResult = d1 - d2;
                        break;
                    case INST_MULT:
                        dResult = d1 * d2;
                        break;
                    case INST_DIV:
#ifndef IEEE_FLOATING_POINT
                        if (d2 == 0.0) {
                            return DIVIDED_BY_ZERO;
                        }
#endif
                        /*
                         * We presume that we are running with zero-divide unmasked if
                         * we're on an IEEE box. Otherwise, this statement might cause
                         * demons to fly out our noses.
                         */

                        dResult = d1 / d2;
                        break;
                    default:
                        /* Unused, here to silence compiler warning. */
                        dResult = 0;
                }

                return Tcl_NewDoubleObj(dResult);
            }
            if ((a_type == TCL_NUMBER_INT) && (b_type == TCL_NUMBER_INT)) {
                w1 = *((const Tcl_WideInt *) a_val);
                w2 = *((const Tcl_WideInt *) b_val);

                switch (opcode) {
                    case INST_ADD:
                        wResult = (Tcl_WideInt) ((Tcl_WideUInt) w1 + (Tcl_WideUInt) w2);
                        if ((a_type == TCL_NUMBER_INT) || (b_type == TCL_NUMBER_INT)) {
                            /*
                             * Check for overflow.
                             */

                            if (Overflowing(w1, w2, wResult)) {
                                return GENERAL_ARITHMETIC_ERROR;
                            }
                        }
                        break;

                    case INST_SUB:
                        wResult = (Tcl_WideInt) ((Tcl_WideUInt) w1 - (Tcl_WideUInt) w2);
                        if ((a_type == TCL_NUMBER_INT) || (b_type == TCL_NUMBER_INT)) {
                            /*
                             * Must check for overflow. The macro tests for overflows
                             * in sums by looking at the sign bits. As we have a
                             * subtraction here, we are adding -w2. As -w2 could in
                             * turn overflow, we test with ~w2 instead: it has the
                             * opposite sign bit to w2 so it does the job. Note that
                             * the only "bad" case (w2==0) is irrelevant for this
                             * macro, as in that case w1 and wResult have the same
                             * sign and there is no overflow anyway.
                             */

                            if (Overflowing(w1, ~w2, wResult)) {
                                return GENERAL_ARITHMETIC_ERROR;
                            }
                        }
                        break;

                    case INST_MULT:
                        if ((w1 < INT_MIN) || (w1 > INT_MAX) || (w2 < INT_MIN) || (w2 > INT_MAX)) {
                            return GENERAL_ARITHMETIC_ERROR;
                        }
                        wResult = w1 * w2;
                        break;

                    case INST_DIV:
                        if (w2 == 0) {
                            return DIVIDED_BY_ZERO;
                        }

                        /*
                         * Need a bignum to represent (WIDE_MIN / -1)
                         */

                        if ((w1 == WIDE_MIN) && (w2 == -1)) {
                            return GENERAL_ARITHMETIC_ERROR;
                        }
                        wResult = w1 / w2;

                        /*
                         * Force Tcl's integer division rules.
                         * TODO: examine for logic simplification
                         */

                        if (((wResult < 0) || ((wResult == 0) &&
                                               ((w1 < 0 && w2 > 0) || (w1 > 0 && w2 < 0)))) &&
                            (wResult * w2 != w1)) {
                            wResult -= 1;
                        }
                        break;

                    default:
                        /*
                         * Unused, here to silence compiler warning.
                         */

                        wResult = 0;
                }

                return Tcl_NewWideIntObj(wResult);
            }
            return GENERAL_ARITHMETIC_ERROR;
    }

    Tcl_Panic("unexpected opcode");
    return NULL;
}

Tcl_Obj *__thtml_add__(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *b) {
    return __thtml_binary_math_op__(interp, INST_ADD, a, b);
}
Tcl_Obj *__thtml_sub__(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *b) {
    return __thtml_binary_math_op__(interp, INST_SUB, a, b);
}
Tcl_Obj *__thtml_mult__(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *b) {
    return __thtml_binary_math_op__(interp, INST_MULT, a, b);
}
Tcl_Obj *__thtml_div__(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *b) {
    return __thtml_binary_math_op__(interp, INST_DIV, a, b);
}
Tcl_Obj *__thtml_mod__(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *b) {
    return __thtml_binary_math_op__(interp, INST_MOD, a, b);
}
Tcl_Obj *__thtml_lshift__(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *b) {
    return __thtml_binary_math_op__(interp, INST_LSHIFT, a, b);
}
Tcl_Obj *__thtml_rshift__(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *b) {
    return __thtml_binary_math_op__(interp, INST_RSHIFT, a, b);
}
Tcl_Obj *__thtml_bitor__(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *b) {
    return __thtml_binary_math_op__(interp, INST_BITOR, a, b);
}
Tcl_Obj *__thtml_bitxor__(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *b) {
    return __thtml_binary_math_op__(interp, INST_BITXOR, a, b);
}
Tcl_Obj *__thtml_bitand__(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *b) {
    return __thtml_binary_math_op__(interp, INST_BITAND, a, b);
}
Tcl_Obj *__thtml_expon__(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *b) {
    return __thtml_binary_math_op__(interp, INST_EXPON, a, b);
}

static Tcl_Obj *__thtml_unary_math_op(
        int opcode,			/* What operation to perform. */
        Tcl_Obj *valuePtr)		/* The operand on the stack. */
{
    void *ptr = NULL;
    int type;
    Tcl_WideInt w;

    (void) Tcl_GetNumberFromObj(NULL, valuePtr, &ptr, &type);

    switch (opcode) {
        case INST_BITNOT:
            if (type == TCL_NUMBER_INT) {
                w = *((const Tcl_WideInt *) ptr);
                return Tcl_NewWideIntObj(~w);
            }
            return OUT_OF_MEMORY;
        case INST_UMINUS:
            switch (type) {
                case TCL_NUMBER_DOUBLE:
                    return Tcl_NewDoubleObj(-(*((const double *) ptr)));
                case TCL_NUMBER_INT:
                    w = *((const Tcl_WideInt *) ptr);
                    if (w != WIDE_MIN) {
                        return Tcl_NewWideIntObj(-w);
                    }
                    return OUT_OF_MEMORY;
                    break;
            }
            return OUT_OF_MEMORY;
    }

    Tcl_Panic("unexpected opcode");
    return NULL;
}

Tcl_Obj *__thtml_bitnot__(Tcl_Obj *a) {
    return __thtml_unary_math_op(INST_BITNOT, a);
}

Tcl_Obj *__thtml_uminus__(Tcl_Obj *a) {
    return __thtml_unary_math_op(INST_UMINUS, a);
}

Tcl_Obj *__thtml_ternary__(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *b, Tcl_Obj *c) {
    int a_val;
    if (Tcl_GetBooleanFromObj(interp, a, &a_val) != TCL_OK) {
        return GENERAL_ARITHMETIC_ERROR;
    }
    return a_val ? b : c;
}

#endif // THTML_H