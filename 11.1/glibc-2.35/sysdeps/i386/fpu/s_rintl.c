/*
 * Public domain.
 */

#define NO_MATH_REDIRECT
#include <libm-alias-ldouble.h>

long double
__rintl (long double x)
{
  long double res;

  asm ("frndint" : "=t" (res) : "0" (x));
  return res;
}

libm_alias_ldouble (__rint, rint)
