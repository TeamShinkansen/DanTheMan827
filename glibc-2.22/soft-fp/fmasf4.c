/* Implement fmaf using soft-fp.
   Copyright (C) 2013-2015 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   In addition to the permissions in the GNU Lesser General Public
   License, the Free Software Foundation gives you unlimited
   permission to link the compiled version of this file into
   combinations with other programs, and to distribute those
   combinations without any restriction coming from the use of this
   file.  (The Lesser General Public License restrictions do apply in
   other respects; for example, they cover modification of the file,
   and distribution when not linked into a combine executable.)

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#include <libc-internal.h>
#include <math.h>
#include "soft-fp.h"
#include "single.h"

float
__fmaf (float a, float b, float c)
{
  FP_DECL_EX;
  FP_DECL_S (A);
  FP_DECL_S (B);
  FP_DECL_S (C);
  FP_DECL_S (R);
  float r;

  FP_INIT_ROUNDMODE;
  FP_UNPACK_S (A, a);
  FP_UNPACK_S (B, b);
  FP_UNPACK_S (C, c);
  FP_FMA_S (R, A, B, C);
  /* R_e is not set in cases where it is not used in packing, but the
     compiler does not see that it is set in all cases where it is
     used, resulting in warnings that it may be used
     uninitialized.  */
  DIAG_PUSH_NEEDS_COMMENT;
#if __GNUC_PREREQ (4, 7)
  DIAG_IGNORE_NEEDS_COMMENT (4.9, "-Wmaybe-uninitialized");
#else
  DIAG_IGNORE_NEEDS_COMMENT (4.9, "-Wuninitialized");
#endif
  FP_PACK_S (r, R);
  DIAG_POP_NEEDS_COMMENT;
  FP_HANDLE_EXCEPTIONS;

  return r;
}
#ifndef __fmaf
weak_alias (__fmaf, fmaf)
#endif