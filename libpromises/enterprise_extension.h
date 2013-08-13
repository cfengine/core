/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

/*******************************************************************************
 * Note: This shared library calling mechanism should only be used for private
 * shared libraries, such as the Enterprise plugins for CFEngine.
 ******************************************************************************/

/*******************************************************************************
 * How to use the Enterprise calling API:
 *
 * In core:
 *
 * - Declare function prototypes using the ENTERPRISE_FUNC_xARG_DECLARE macros.
 *   Replace x with the number of arguments to the function.
 *
 * - Define a stub function using the ENTERPRISE_FUNC_xARG_DEFINE_STUB macros.
 *
 * In Enterprise:
 *
 * - Include the same prototype as you used in core.
 *
 * - Define the Enterprise function with the ENTERPRISE_FUNC_xARG_DEFINE
 *   macros.
 *
 * IMPORTANT:
 *
 * - For functions returning void, you need to use the VOID_FUNC version of the
 *   macros.
 *
 * - Due to macro limitations, for each function argument, you need to put the
 *   type and the name as separate arguments, so instead of (int par), use
 *   (int, par).
 *
 * - You can use the function normally in your code. If the Enterprise plugin
 *   is available, it will call that function, if not, it will call the stub
 *   function.
 *
 * - The lookup is more expensive than a normal function call. Don't use it in
 *   a tight loop.
 *
 * - Be careful when changing the function signature. Core and Enterprise need
 *   to stay binary compatible, otherwise the plugin will not work, and may even
 *   crash. Signature changes should only happen between major releases.
 *
 * Examples:
 *
 * - int EnterpriseOnlyInt(const char *str, int num) becomes
 *     ENTERPRISE_FUNC_2ARG_DECLARE(int, EnterpriseOnlyInt,
 *                                  const char *, str, int, num)
 *     ENTERPRISE_FUNC_2ARG_DEFINE_STUB(int, EnterpriseOnlyInt,
 *                                      const char *, str, int, num)
 *     ENTERPRISE_FUNC_2ARG_DEFINE(int, EnterpriseOnlyInt,
 *                                 const char *, str, int, num)
 *
 * - void EnterpriseFunc(int num) becomes
 *     ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, EnterpriseFunc, int, num)
 *     ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(void, EnterpriseFunc, int, num)
 *     ENTERPRISE_VOID_FUNC_1ARG_DEFINE(void, EnterpriseFunc, int, num)
 ******************************************************************************/


/*******************************************************************************
 * It may be easier to understand the implementation below by looking at the
 * output it actually produces. gcc -E is your friend.
 ******************************************************************************/

#ifndef ENTERPRISE_FUNCTION_CALL_H
#define ENTERPRISE_FUNCTION_CALL_H

#include <logging.h>

#ifndef BUILTIN_EXTENSIONS

#define ENTERPRISE_CANARY_VALUE 0x10203040
#define ENTERPRISE_LIBRARY_NAME "cfengine-enterprise.so"

void *enterprise_library_open();
void enterprise_library_close(void *handle);

void *shlib_open(const char *lib_name);
void *shlib_load(void *handle, const char *symbol_name);
void shlib_close(void *handle);

#ifndef BUILDING_CORE_EXTENSION
# define ENTERPRISE_FUNC_0ARG_WRAPPER_SIGNATURE(__ret, __func) \
    __ret __func()
# define ENTERPRISE_FUNC_1ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1) \
    __ret __func(__t1 __p1)
# define ENTERPRISE_FUNC_2ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2) \
    __ret __func(__t1 __p1, __t2 __p2)
# define ENTERPRISE_FUNC_3ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3)
# define ENTERPRISE_FUNC_4ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4)
# define ENTERPRISE_FUNC_5ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5)
# define ENTERPRISE_FUNC_6ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6)
# define ENTERPRISE_FUNC_7ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7)
# define ENTERPRISE_FUNC_8ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8)
# define ENTERPRISE_FUNC_9ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9)
# define ENTERPRISE_FUNC_10ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10)
# define ENTERPRISE_FUNC_11ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11)
# define ENTERPRISE_FUNC_12ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12)
# define ENTERPRISE_FUNC_13ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13)
# define ENTERPRISE_FUNC_14ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14)
# define ENTERPRISE_FUNC_15ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14, __t15 __p15)

# define ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func) \
    __ret __func##__stub()
# define ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1) \
    __ret __func##__stub(__t1 __p1)
# define ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2) \
    __ret __func##__stub(__t1 __p1, __t2 __p2)
# define ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3)
# define ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4)
# define ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5)
# define ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6)
# define ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7)
# define ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8)
# define ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9)
# define ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10)
# define ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11)
# define ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12)
# define ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13)
# define ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14)
# define ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14, __t15 __p15)

#else // BUILDING_CORE_EXTENSION

# define ENTERPRISE_FUNC_0ARG_WRAPPER_SIGNATURE(__ret, __func) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, int32_t __end_canary)
# define ENTERPRISE_FUNC_1ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, int32_t __end_canary)
# define ENTERPRISE_FUNC_2ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, int32_t __end_canary)
# define ENTERPRISE_FUNC_3ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, int32_t __end_canary)
# define ENTERPRISE_FUNC_4ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, int32_t __end_canary)
# define ENTERPRISE_FUNC_5ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, int32_t __end_canary)
# define ENTERPRISE_FUNC_6ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, int32_t __end_canary)
# define ENTERPRISE_FUNC_7ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, int32_t __end_canary)
# define ENTERPRISE_FUNC_8ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, int32_t __end_canary)
# define ENTERPRISE_FUNC_9ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, int32_t __end_canary)
# define ENTERPRISE_FUNC_10ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, int32_t __end_canary)
# define ENTERPRISE_FUNC_11ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, int32_t __end_canary)
# define ENTERPRISE_FUNC_12ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, int32_t __end_canary)
# define ENTERPRISE_FUNC_13ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, int32_t __end_canary)
# define ENTERPRISE_FUNC_14ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14, int32_t __end_canary)
# define ENTERPRISE_FUNC_15ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    __ret __func##__wrapper(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14, __t15 __p15, int32_t __end_canary)

# define ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func) \
    __ret __func##__real()
# define ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1) \
    __ret __func##__real(__t1 __p1)
# define ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2) \
    __ret __func##__real(__t1 __p1, __t2 __p2)
# define ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    __ret __func##__real(__t1 __p1, __t2 __p2, __t3 __p3)
# define ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    __ret __func##__real(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4)
# define ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    __ret __func##__real(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5)
# define ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    __ret __func##__real(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6)
# define ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    __ret __func##__real(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7)
# define ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    __ret __func##__real(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8)
# define ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    __ret __func##__real(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9)
# define ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    __ret __func##__real(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10)
# define ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    __ret __func##__real(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11)
# define ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    __ret __func##__real(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12)
# define ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    __ret __func##__real(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13)
# define ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    __ret __func##__real(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14)
# define ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    __ret __func##__real(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14, __t15 __p15)

# define ENTERPRISE_FUNC_0ARG_INLINE_SIGNATURE(__ret, __func) \
    inline static __ret __func()
# define ENTERPRISE_FUNC_1ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1) \
    inline static __ret __func(__t1 __p1)
# define ENTERPRISE_FUNC_2ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2) \
    inline static __ret __func(__t1 __p1, __t2 __p2)
# define ENTERPRISE_FUNC_3ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    inline static __ret __func(__t1 __p1, __t2 __p2, __t3 __p3)
# define ENTERPRISE_FUNC_4ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    inline static __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4)
# define ENTERPRISE_FUNC_5ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    inline static __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5)
# define ENTERPRISE_FUNC_6ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    inline static __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6)
# define ENTERPRISE_FUNC_7ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    inline static __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7)
# define ENTERPRISE_FUNC_8ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    inline static __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8)
# define ENTERPRISE_FUNC_9ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    inline static __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9)
# define ENTERPRISE_FUNC_10ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    inline static __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10)
# define ENTERPRISE_FUNC_11ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    inline static __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11)
# define ENTERPRISE_FUNC_12ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    inline static __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12)
# define ENTERPRISE_FUNC_13ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    inline static __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13)
# define ENTERPRISE_FUNC_14ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    inline static __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14)
# define ENTERPRISE_FUNC_15ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    inline static __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14, __t15 __p15)

#endif // BUILDING_CORE_EXTENSION

#ifndef BUILDING_CORE_EXTENSION

// The __ret__assign and __ret_ref parameters are to work around functions returning void.
#define ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, __real__func__par, __stub__func__par) \
    { \
        void *__handle = enterprise_library_open(); \
        if (__handle) \
        { \
            __func##__type func_ptr = shlib_load(__handle, #__func "__wrapper"); \
            if (func_ptr) \
            { \
                int __successful = 0; \
                __ret__assign func_ptr __real__func__par;     \
                if (__successful) \
                { \
                    enterprise_library_close(__handle); \
                    return __ret__ref; \
                } \
            } \
            enterprise_library_close(__handle); \
        } \
        return __func##__stub __stub__func__par;  \
    }

# define ENTERPRISE_FUNC_0ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, int32_t __end_canary); \
    ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func); \
    inline static ENTERPRISE_FUNC_0ARG_WRAPPER_SIGNATURE(__ret, __func) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, ENTERPRISE_CANARY_VALUE), \
                                ()) \
    ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func)

# define ENTERPRISE_FUNC_1ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, int32_t __end_canary); \
    ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1); \
    inline static ENTERPRISE_FUNC_1ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, ENTERPRISE_CANARY_VALUE), \
                                (__p1)) \
    ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1)

# define ENTERPRISE_FUNC_2ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1, __t2, __p2) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, int32_t __end_canary); \
    ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2); \
    inline static ENTERPRISE_FUNC_2ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, __p2, ENTERPRISE_CANARY_VALUE), \
                                (__p1, __p2)) \
    ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2)

# define ENTERPRISE_FUNC_3ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1, __t2, __p2, __t3, __p3) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, int32_t __end_canary); \
    ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3); \
    inline static ENTERPRISE_FUNC_3ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, __p2, __p3, ENTERPRISE_CANARY_VALUE), \
                                (__p1, __p2, __p3)) \
    ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3)

# define ENTERPRISE_FUNC_4ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, int32_t __end_canary); \
    ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4); \
    inline static ENTERPRISE_FUNC_4ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, __p2, __p3, __p4, ENTERPRISE_CANARY_VALUE), \
                                (__p1, __p2, __p3, __p4)) \
    ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)

# define ENTERPRISE_FUNC_5ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, int32_t __end_canary); \
    ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5); \
    inline static ENTERPRISE_FUNC_5ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, __p2, __p3, __p4, __p5, ENTERPRISE_CANARY_VALUE), \
                                (__p1, __p2, __p3, __p4, __p5)) \
    ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)

# define ENTERPRISE_FUNC_6ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, int32_t __end_canary); \
    ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6); \
    inline static ENTERPRISE_FUNC_6ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, __p2, __p3, __p4, __p5, __p6, ENTERPRISE_CANARY_VALUE), \
                                (__p1, __p2, __p3, __p4, __p5, __p6)) \
    ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)

# define ENTERPRISE_FUNC_7ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, int32_t __end_canary); \
    ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7); \
    inline static ENTERPRISE_FUNC_7ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, __p2, __p3, __p4, __p5, __p6, __p7, ENTERPRISE_CANARY_VALUE), \
                                (__p1, __p2, __p3, __p4, __p5, __p6, __p7)) \
    ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)

# define ENTERPRISE_FUNC_8ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, int32_t __end_canary); \
    ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8); \
    inline static ENTERPRISE_FUNC_8ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, ENTERPRISE_CANARY_VALUE), \
                                (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8)) \
    ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)

# define ENTERPRISE_FUNC_9ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, int32_t __end_canary); \
    ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9); \
    inline static ENTERPRISE_FUNC_9ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, ENTERPRISE_CANARY_VALUE), \
                                (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9)) \
    ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)
# define ENTERPRISE_FUNC_10ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, int32_t __end_canary); \
    ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10); \
    inline static ENTERPRISE_FUNC_10ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, ENTERPRISE_CANARY_VALUE), \
                                (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10)) \
    ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)
# define ENTERPRISE_FUNC_11ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, int32_t __end_canary); \
    ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11); \
    inline static ENTERPRISE_FUNC_11ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, ENTERPRISE_CANARY_VALUE), \
                                (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11)) \
    ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)
# define ENTERPRISE_FUNC_12ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, int32_t __end_canary); \
    ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12); \
    inline static ENTERPRISE_FUNC_12ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, ENTERPRISE_CANARY_VALUE), \
                                (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12)) \
    ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)
# define ENTERPRISE_FUNC_13ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, int32_t __end_canary); \
    ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13); \
    inline static ENTERPRISE_FUNC_13ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13, ENTERPRISE_CANARY_VALUE), \
                                (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13)) \
    ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)
# define ENTERPRISE_FUNC_14ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14, int32_t __end_canary); \
    ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14); \
    inline static ENTERPRISE_FUNC_14ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13, __p14, ENTERPRISE_CANARY_VALUE), \
                                (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13, __p14)) \
    ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)
# define ENTERPRISE_FUNC_15ARG_DECLARE_IMPL(__ret, __func, __ret__assign, __ret__ref, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    typedef __ret (*__func##__type)(int32_t __start_canary, int *__successful, __t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14, __t15 __p15, int32_t __end_canary); \
    ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15); \
    inline static ENTERPRISE_FUNC_15ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
        ENTERPRISE_FUNC_IMPL_LOADER(__ret, __func, __ret__assign, __ret__ref, \
                                (ENTERPRISE_CANARY_VALUE, &__successful, __p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13, __p14, __p15, ENTERPRISE_CANARY_VALUE), \
                                (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13, __p14, __p15)) \
    ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)


# define ENTERPRISE_FUNC_0ARG_DECLARE(__ret, __func) \
    ENTERPRISE_FUNC_0ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value)
# define ENTERPRISE_FUNC_1ARG_DECLARE(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_1ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1)
# define ENTERPRISE_FUNC_2ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_2ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1, __t2, __p2)
# define ENTERPRISE_FUNC_3ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_3ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1, __t2, __p2, __t3, __p3)
# define ENTERPRISE_FUNC_4ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_4ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)
# define ENTERPRISE_FUNC_5ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_5ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)
# define ENTERPRISE_FUNC_6ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_6ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)
# define ENTERPRISE_FUNC_7ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_7ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)
# define ENTERPRISE_FUNC_8ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_8ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)
# define ENTERPRISE_FUNC_9ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_9ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)
# define ENTERPRISE_FUNC_10ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_10ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)
# define ENTERPRISE_FUNC_11ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_11ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)
# define ENTERPRISE_FUNC_12ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_12ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)
# define ENTERPRISE_FUNC_13ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_13ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)
# define ENTERPRISE_FUNC_14ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_14ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)
# define ENTERPRISE_FUNC_15ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_15ARG_DECLARE_IMPL(__ret, __func, __ret __ret_value =, __ret_value, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)

# define ENTERPRISE_VOID_FUNC_0ARG_DECLARE(__ret, __func) \
    ENTERPRISE_FUNC_0ARG_DECLARE_IMPL(__ret, __func, , )
# define ENTERPRISE_VOID_FUNC_1ARG_DECLARE(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_1ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1)
# define ENTERPRISE_VOID_FUNC_2ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_2ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1, __t2, __p2)
# define ENTERPRISE_VOID_FUNC_3ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_3ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1, __t2, __p2, __t3, __p3)
# define ENTERPRISE_VOID_FUNC_4ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_4ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)
# define ENTERPRISE_VOID_FUNC_5ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_5ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)
# define ENTERPRISE_VOID_FUNC_6ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_6ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)
# define ENTERPRISE_VOID_FUNC_7ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_7ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)
# define ENTERPRISE_VOID_FUNC_8ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_8ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)
# define ENTERPRISE_VOID_FUNC_9ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_9ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)
# define ENTERPRISE_VOID_FUNC_10ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_10ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)
# define ENTERPRISE_VOID_FUNC_11ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_11ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)
# define ENTERPRISE_VOID_FUNC_12ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_12ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)
# define ENTERPRISE_VOID_FUNC_13ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_13ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)
# define ENTERPRISE_VOID_FUNC_14ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_14ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)
# define ENTERPRISE_VOID_FUNC_15ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_15ARG_DECLARE_IMPL(__ret, __func, , , __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)

#else // BUILDING_CORE_EXTENSION

# define ENTERPRISE_FUNC_0ARG_DECLARE(__ret, __func) \
    ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func); \
    ENTERPRISE_FUNC_0ARG_INLINE_SIGNATURE(__ret, __func) \
    { \
        return __func##__real(); \
    } \
    ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func)
# define ENTERPRISE_FUNC_1ARG_DECLARE(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1); \
    ENTERPRISE_FUNC_1ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1) \
    { \
        return __func##__real(__p1); \
    } \
    ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1)
# define ENTERPRISE_FUNC_2ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2); \
    ENTERPRISE_FUNC_2ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2) \
    { \
        return __func##__real(__p1, __p2); \
    } \
    ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2)
# define ENTERPRISE_FUNC_3ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3); \
    ENTERPRISE_FUNC_3ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    { \
        return __func##__real(__p1, __p2, __p3); \
    } \
    ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3)
# define ENTERPRISE_FUNC_4ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4); \
    ENTERPRISE_FUNC_4ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4); \
    } \
    ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)
# define ENTERPRISE_FUNC_5ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5); \
    ENTERPRISE_FUNC_5ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5); \
    } \
    ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)
# define ENTERPRISE_FUNC_6ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6); \
    ENTERPRISE_FUNC_6ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6); \
    } \
    ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)
# define ENTERPRISE_FUNC_7ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7); \
    ENTERPRISE_FUNC_7ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7); \
    } \
    ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)
# define ENTERPRISE_FUNC_8ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8); \
    ENTERPRISE_FUNC_8ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8); \
    } \
    ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)
# define ENTERPRISE_FUNC_9ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9); \
    ENTERPRISE_FUNC_9ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9); \
    } \
    ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)
# define ENTERPRISE_FUNC_10ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10); \
    ENTERPRISE_FUNC_10ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10); \
    } \
    ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)
# define ENTERPRISE_FUNC_11ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11); \
    ENTERPRISE_FUNC_11ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11); \
    } \
    ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)
# define ENTERPRISE_FUNC_12ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12); \
    ENTERPRISE_FUNC_12ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12); \
    } \
    ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)
# define ENTERPRISE_FUNC_13ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13); \
    ENTERPRISE_FUNC_13ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13); \
    } \
    ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)
# define ENTERPRISE_FUNC_14ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14); \
    ENTERPRISE_FUNC_14ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13, __p14); \
    } \
    ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)
# define ENTERPRISE_FUNC_15ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15); \
    ENTERPRISE_FUNC_15ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13, __p14, __p15); \
    } \
    ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)


# define ENTERPRISE_VOID_FUNC_0ARG_DECLARE(__ret, __func) \
    ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func); \
    ENTERPRISE_FUNC_0ARG_INLINE_SIGNATURE(__ret, __func) \
    { \
        return __func##__real(); \
    } \
    ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func)
# define ENTERPRISE_VOID_FUNC_1ARG_DECLARE(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1); \
    ENTERPRISE_FUNC_1ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1) \
    { \
        return __func##__real(__p1); \
    } \
    ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1)
# define ENTERPRISE_VOID_FUNC_2ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2); \
    ENTERPRISE_FUNC_2ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2) \
    { \
        return __func##__real(__p1, __p2); \
    } \
    ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2)
# define ENTERPRISE_VOID_FUNC_3ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3); \
    ENTERPRISE_FUNC_3ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    { \
        return __func##__real(__p1, __p2, __p3); \
    } \
    ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3)
# define ENTERPRISE_VOID_FUNC_4ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4); \
    ENTERPRISE_FUNC_4ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4); \
    } \
    ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)
# define ENTERPRISE_VOID_FUNC_5ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5); \
    ENTERPRISE_FUNC_5ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5); \
    } \
    ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)
# define ENTERPRISE_VOID_FUNC_6ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6); \
    ENTERPRISE_FUNC_6ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6); \
    } \
    ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)
# define ENTERPRISE_VOID_FUNC_7ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7); \
    ENTERPRISE_FUNC_7ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7); \
    } \
    ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)
# define ENTERPRISE_VOID_FUNC_8ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8); \
    ENTERPRISE_FUNC_8ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8); \
    } \
    ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)
# define ENTERPRISE_VOID_FUNC_9ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9); \
    ENTERPRISE_FUNC_9ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9); \
    } \
    ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)
# define ENTERPRISE_VOID_FUNC_10ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10); \
    ENTERPRISE_FUNC_10ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10); \
    } \
    ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)
# define ENTERPRISE_VOID_FUNC_11ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11); \
    ENTERPRISE_FUNC_11ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11); \
    } \
    ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)
# define ENTERPRISE_VOID_FUNC_12ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12); \
    ENTERPRISE_FUNC_12ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12); \
    } \
    ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)
# define ENTERPRISE_VOID_FUNC_13ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13); \
    ENTERPRISE_FUNC_13ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13); \
    } \
    ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)
# define ENTERPRISE_VOID_FUNC_14ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14); \
    ENTERPRISE_FUNC_14ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13, __p14); \
    } \
    ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)
# define ENTERPRISE_VOID_FUNC_15ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15); \
    ENTERPRISE_FUNC_15ARG_INLINE_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    { \
        return __func##__real(__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13, __p14, __p15); \
    } \
    ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)

#endif // BUILDING_CORE_EXTENSION


#ifndef BUILDING_CORE_EXTENSION

#define ENTERPRISE_FUNC_0ARG_DEFINE_STUB(__ret, __func) \
    ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func)
#define ENTERPRISE_FUNC_1ARG_DEFINE_STUB(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1)
#define ENTERPRISE_FUNC_2ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2)
#define ENTERPRISE_FUNC_3ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3)
#define ENTERPRISE_FUNC_4ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)
#define ENTERPRISE_FUNC_5ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)
#define ENTERPRISE_FUNC_6ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)
#define ENTERPRISE_FUNC_7ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)
#define ENTERPRISE_FUNC_8ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)
#define ENTERPRISE_FUNC_9ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)
#define ENTERPRISE_FUNC_10ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)
#define ENTERPRISE_FUNC_11ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)
#define ENTERPRISE_FUNC_12ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)
#define ENTERPRISE_FUNC_13ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)
#define ENTERPRISE_FUNC_14ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)
#define ENTERPRISE_FUNC_15ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)

#define ENTERPRISE_VOID_FUNC_0ARG_DEFINE_STUB(__ret, __func) \
    ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func)
#define ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1)
#define ENTERPRISE_VOID_FUNC_2ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2)
#define ENTERPRISE_VOID_FUNC_3ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3)
#define ENTERPRISE_VOID_FUNC_4ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)
#define ENTERPRISE_VOID_FUNC_5ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)
#define ENTERPRISE_VOID_FUNC_6ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)
#define ENTERPRISE_VOID_FUNC_7ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)
#define ENTERPRISE_VOID_FUNC_8ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)
#define ENTERPRISE_VOID_FUNC_9ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)
#define ENTERPRISE_VOID_FUNC_10ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)
#define ENTERPRISE_VOID_FUNC_11ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)
#define ENTERPRISE_VOID_FUNC_12ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)
#define ENTERPRISE_VOID_FUNC_13ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)
#define ENTERPRISE_VOID_FUNC_14ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)
#define ENTERPRISE_VOID_FUNC_15ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)

#define ENTERPRISE_FUNC_DEFINE_REAL_INVALID In_core_code_you_need_to_use_DEFINE_STUB func()

#define ENTERPRISE_FUNC_0ARG_DEFINE(__ret, __func) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_1ARG_DEFINE(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_2ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_3ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_4ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_5ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_6ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_7ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_8ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_9ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_10ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_11ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_12ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_13ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_14ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_FUNC_15ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID

#define ENTERPRISE_VOID_FUNC_0ARG_DEFINE(__ret, __func) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_1ARG_DEFINE(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_2ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_3ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_4ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_5ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_6ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_7ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_8ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_9ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_10ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_11ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_12ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_13ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_14ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID
#define ENTERPRISE_VOID_FUNC_15ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_DEFINE_REAL_INVALID

#else // BUILDING_CORE_EXTENSION

#define ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL_RET_VALUE(__ret, __func, __real__func__par, __ret__expr) \
    { \
        if (__start_canary != ENTERPRISE_CANARY_VALUE || __end_canary != ENTERPRISE_CANARY_VALUE) \
        { \
            Log(LOG_LEVEL_ERR, "Function '%s %s%s' failed stack consistency check. Most likely this means the plugin containing the " \
                               "function is incompatible with this version of CFEngine.", #__ret, #__func, #__real__func__par); \
            __ret__expr; \
        } \
        *__successful = 1; \
        return __func __real__func__par; \
    }

#define ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, __real__func__par) \
    ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL_RET_VALUE(__ret, __func, __real__func__par, \
        __ret __ret_value = __ret_value; \
        (void)__ret_value; \
        return __ret_value)

#define ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, __real__func__par) \
    ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL_RET_VALUE(__ret, __func, __real__func__par, return)

#define ENTERPRISE_FUNC_DEFINE_INLINE_IMPL(__ret, __func, __real__func__par) \
    { \
        return __func##__real __real__func__par; \
    }

#define ENTERPRISE_FUNC_0ARG_DEFINE(__ret, __func) \
    ENTERPRISE_FUNC_0ARG_WRAPPER_SIGNATURE(__ret, __func) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   ()) \
    ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func)

#define ENTERPRISE_FUNC_1ARG_DEFINE(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_1ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1)) \
    ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1)

#define ENTERPRISE_FUNC_2ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_2ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2)) \
    ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2)

#define ENTERPRISE_FUNC_3ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_3ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3)) \
    ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3)

#define ENTERPRISE_FUNC_4ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_4ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4)) \
    ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)

#define ENTERPRISE_FUNC_5ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_5ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5)) \
    ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)

#define ENTERPRISE_FUNC_6ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_6ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6)) \
    ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)

#define ENTERPRISE_FUNC_7ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_7ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7)) \
    ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)

#define ENTERPRISE_FUNC_8ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_8ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8)) \
    ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)

#define ENTERPRISE_FUNC_9ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_9ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9)) \
    ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)

#define ENTERPRISE_FUNC_10ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_10ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10)) \
    ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)

#define ENTERPRISE_FUNC_11ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_11ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11)) \
    ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)

#define ENTERPRISE_FUNC_12ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_12ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12)) \
    ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)

#define ENTERPRISE_FUNC_13ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_13ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13)) \
    ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)

#define ENTERPRISE_FUNC_14ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_14ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13, __p14)) \
    ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)

#define ENTERPRISE_FUNC_15ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_15ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
        ENTERPRISE_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13, __p14, __p15)) \
    ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)


#define ENTERPRISE_VOID_FUNC_0ARG_DEFINE(__ret, __func) \
    ENTERPRISE_FUNC_0ARG_WRAPPER_SIGNATURE(__ret, __func) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   ()) \
    ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func)

#define ENTERPRISE_VOID_FUNC_1ARG_DEFINE(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_1ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1)) \
    ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1)

#define ENTERPRISE_VOID_FUNC_2ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_2ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2)) \
    ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2)

#define ENTERPRISE_VOID_FUNC_3ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_3ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3)) \
    ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3)

#define ENTERPRISE_VOID_FUNC_4ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_4ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4)) \
    ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)

#define ENTERPRISE_VOID_FUNC_5ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_5ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5)) \
    ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)

#define ENTERPRISE_VOID_FUNC_6ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_6ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6)) \
    ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)

#define ENTERPRISE_VOID_FUNC_7ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_7ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7)) \
    ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)

#define ENTERPRISE_VOID_FUNC_8ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_8ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8)) \
    ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)

#define ENTERPRISE_VOID_FUNC_9ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_9ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9)) \
    ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)

#define ENTERPRISE_VOID_FUNC_10ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_10ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10)) \
    ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)

#define ENTERPRISE_VOID_FUNC_11ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_11ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11)) \
    ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)

#define ENTERPRISE_VOID_FUNC_12ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_12ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12)) \
    ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)

#define ENTERPRISE_VOID_FUNC_13ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_13ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13)) \
    ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)

#define ENTERPRISE_VOID_FUNC_14ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_14ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13, __p14)) \
    ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)

#define ENTERPRISE_VOID_FUNC_15ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_15ARG_WRAPPER_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
        ENTERPRISE_VOID_FUNC_DEFINE_WRAPPER_IMPL(__ret, __func, \
                                   (__p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9, __p10, __p11, __p12, __p13, __p14, __p15)) \
    ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)


#define ENTERPRISE_FUNC_DEFINE_STUB_INVALID In_extension_code_you_cannot_define_a_STUB func()

#define ENTERPRISE_FUNC_0ARG_DEFINE_STUB(__ret, __func)       \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_1ARG_DEFINE_STUB(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_2ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_3ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_4ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_5ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_6ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_7ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_8ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_9ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_10ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_11ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_12ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_13ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_14ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_FUNC_15ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID

#define ENTERPRISE_VOID_FUNC_0ARG_DEFINE_STUB(__ret, __func) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_2ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_3ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_4ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_5ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_6ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_7ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_8ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_9ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_10ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_11ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_12ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_13ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_14ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID
#define ENTERPRISE_VOID_FUNC_15ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_DEFINE_STUB_INVALID

#endif // BUILDING_CORE_EXTENSION


#else // BUILTIN_EXTENSIONS
// In this case just map to real function calls.
// BUILTIN_EXTENSIONS is for Windows binaries and debugging.

# define ENTERPRISE_FUNC_0ARG_STUB_SIGNATURE(__ret, __func) \
    __ret __func##__stub()
# define ENTERPRISE_FUNC_1ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1) \
    __ret __func##__stub(__t1 __p1)
# define ENTERPRISE_FUNC_2ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2) \
    __ret __func##__stub(__t1 __p1, __t2 __p2)
# define ENTERPRISE_FUNC_3ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3)
# define ENTERPRISE_FUNC_4ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4)
# define ENTERPRISE_FUNC_5ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5)
# define ENTERPRISE_FUNC_6ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6)
# define ENTERPRISE_FUNC_7ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7)
# define ENTERPRISE_FUNC_8ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8)
# define ENTERPRISE_FUNC_9ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9)
# define ENTERPRISE_FUNC_10ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10)
# define ENTERPRISE_FUNC_11ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11)
# define ENTERPRISE_FUNC_12ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12)
# define ENTERPRISE_FUNC_13ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13)
# define ENTERPRISE_FUNC_14ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14)
# define ENTERPRISE_FUNC_15ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    __ret __func##__stub(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14, __t15 __p15)

# define ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func) \
    __ret __func()
# define ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1) \
    __ret __func(__t1 __p1)
# define ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2) \
    __ret __func(__t1 __p1, __t2 __p2)
# define ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3)
# define ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4)
# define ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5)
# define ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6)
# define ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7)
# define ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8)
# define ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9)
# define ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10)
# define ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11)
# define ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12)
# define ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13)
# define ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14)
# define ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    __ret __func(__t1 __p1, __t2 __p2, __t3 __p3, __t4 __p4, __t5 __p5, __t6 __p6, __t7 __p7, __t8 __p8, __t9 __p9, __t10 __p10, __t11 __p11, __t12 __p12, __t13 __p13, __t14 __p14, __t15 __p15)


#define ENTERPRISE_FUNC_0ARG_DECLARE(__ret, __func) \
    ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func)
#define ENTERPRISE_FUNC_1ARG_DECLARE(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1)
#define ENTERPRISE_FUNC_2ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2)
#define ENTERPRISE_FUNC_3ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3)
#define ENTERPRISE_FUNC_4ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)
#define ENTERPRISE_FUNC_5ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)
#define ENTERPRISE_FUNC_6ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)
#define ENTERPRISE_FUNC_7ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)
#define ENTERPRISE_FUNC_8ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)
#define ENTERPRISE_FUNC_9ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)
#define ENTERPRISE_FUNC_10ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)
#define ENTERPRISE_FUNC_11ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)
#define ENTERPRISE_FUNC_12ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)
#define ENTERPRISE_FUNC_13ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)
#define ENTERPRISE_FUNC_14ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)
#define ENTERPRISE_FUNC_15ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)

#define ENTERPRISE_VOID_FUNC_0ARG_DECLARE(__ret, __func) \
    ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func)
#define ENTERPRISE_VOID_FUNC_1ARG_DECLARE(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1)
#define ENTERPRISE_VOID_FUNC_2ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2)
#define ENTERPRISE_VOID_FUNC_3ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3)
#define ENTERPRISE_VOID_FUNC_4ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)
#define ENTERPRISE_VOID_FUNC_5ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)
#define ENTERPRISE_VOID_FUNC_6ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)
#define ENTERPRISE_VOID_FUNC_7ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)
#define ENTERPRISE_VOID_FUNC_8ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)
#define ENTERPRISE_VOID_FUNC_9ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)
#define ENTERPRISE_VOID_FUNC_10ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)
#define ENTERPRISE_VOID_FUNC_11ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)
#define ENTERPRISE_VOID_FUNC_12ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)
#define ENTERPRISE_VOID_FUNC_13ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)
#define ENTERPRISE_VOID_FUNC_14ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)
#define ENTERPRISE_VOID_FUNC_15ARG_DECLARE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)

#define ENTERPRISE_FUNC_0ARG_DEFINE(__ret, __func) \
    ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func)
#define ENTERPRISE_FUNC_1ARG_DEFINE(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1)
#define ENTERPRISE_FUNC_2ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2)
#define ENTERPRISE_FUNC_3ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3)
#define ENTERPRISE_FUNC_4ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)
#define ENTERPRISE_FUNC_5ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)
#define ENTERPRISE_FUNC_6ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)
#define ENTERPRISE_FUNC_7ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)
#define ENTERPRISE_FUNC_8ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)
#define ENTERPRISE_FUNC_9ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)
#define ENTERPRISE_FUNC_10ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)
#define ENTERPRISE_FUNC_11ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)
#define ENTERPRISE_FUNC_12ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)
#define ENTERPRISE_FUNC_13ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)
#define ENTERPRISE_FUNC_14ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)
#define ENTERPRISE_FUNC_15ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)

#define ENTERPRISE_VOID_FUNC_0ARG_DEFINE(__ret, __func) \
    ENTERPRISE_FUNC_0ARG_REAL_SIGNATURE(__ret, __func)
#define ENTERPRISE_VOID_FUNC_1ARG_DEFINE(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_1ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1)
#define ENTERPRISE_VOID_FUNC_2ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_2ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2)
#define ENTERPRISE_VOID_FUNC_3ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_3ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3)
#define ENTERPRISE_VOID_FUNC_4ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_4ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)
#define ENTERPRISE_VOID_FUNC_5ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_5ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)
#define ENTERPRISE_VOID_FUNC_6ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_6ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)
#define ENTERPRISE_VOID_FUNC_7ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_7ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)
#define ENTERPRISE_VOID_FUNC_8ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_8ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)
#define ENTERPRISE_VOID_FUNC_9ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_9ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)
#define ENTERPRISE_VOID_FUNC_10ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_10ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)
#define ENTERPRISE_VOID_FUNC_11ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_11ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)
#define ENTERPRISE_VOID_FUNC_12ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_12ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)
#define ENTERPRISE_VOID_FUNC_13ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_13ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)
#define ENTERPRISE_VOID_FUNC_14ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_14ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)
#define ENTERPRISE_VOID_FUNC_15ARG_DEFINE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_15ARG_REAL_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)

#define ENTERPRISE_FUNC_0ARG_DEFINE_STUB(__ret, __func) \
    ENTERPRISE_FUNC_0ARG_STUB_SIGNATURE(__ret, __func)
#define ENTERPRISE_FUNC_1ARG_DEFINE_STUB(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_1ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1)
#define ENTERPRISE_FUNC_2ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_2ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2)
#define ENTERPRISE_FUNC_3ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_3ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3)
#define ENTERPRISE_FUNC_4ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_4ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)
#define ENTERPRISE_FUNC_5ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_5ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)
#define ENTERPRISE_FUNC_6ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_6ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)
#define ENTERPRISE_FUNC_7ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_7ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)
#define ENTERPRISE_FUNC_8ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_8ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)
#define ENTERPRISE_FUNC_9ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_9ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)
#define ENTERPRISE_FUNC_10ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_10ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)
#define ENTERPRISE_FUNC_11ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_11ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)
#define ENTERPRISE_FUNC_12ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_12ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)
#define ENTERPRISE_FUNC_13ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_13ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)
#define ENTERPRISE_FUNC_14ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_14ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)
#define ENTERPRISE_FUNC_15ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_15ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)

#define ENTERPRISE_VOID_FUNC_0ARG_DEFINE_STUB(__ret, __func) \
    ENTERPRISE_FUNC_0ARG_STUB_SIGNATURE(__ret, __func)
#define ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(__ret, __func, __t1, __p1) \
    ENTERPRISE_FUNC_1ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1)
#define ENTERPRISE_VOID_FUNC_2ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2) \
    ENTERPRISE_FUNC_2ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2)
#define ENTERPRISE_VOID_FUNC_3ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3) \
    ENTERPRISE_FUNC_3ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3)
#define ENTERPRISE_VOID_FUNC_4ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4) \
    ENTERPRISE_FUNC_4ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4)
#define ENTERPRISE_VOID_FUNC_5ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5) \
    ENTERPRISE_FUNC_5ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5)
#define ENTERPRISE_VOID_FUNC_6ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6) \
    ENTERPRISE_FUNC_6ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6)
#define ENTERPRISE_VOID_FUNC_7ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7) \
    ENTERPRISE_FUNC_7ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7)
#define ENTERPRISE_VOID_FUNC_8ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8) \
    ENTERPRISE_FUNC_8ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8)
#define ENTERPRISE_VOID_FUNC_9ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9) \
    ENTERPRISE_FUNC_9ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9)
#define ENTERPRISE_VOID_FUNC_10ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10) \
    ENTERPRISE_FUNC_10ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10)
#define ENTERPRISE_VOID_FUNC_11ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11) \
    ENTERPRISE_FUNC_11ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11)
#define ENTERPRISE_VOID_FUNC_12ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12) \
    ENTERPRISE_FUNC_12ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12)
#define ENTERPRISE_VOID_FUNC_13ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13) \
    ENTERPRISE_FUNC_13ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13)
#define ENTERPRISE_VOID_FUNC_14ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14) \
    ENTERPRISE_FUNC_14ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14)
#define ENTERPRISE_VOID_FUNC_15ARG_DEFINE_STUB(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15) \
    ENTERPRISE_FUNC_15ARG_STUB_SIGNATURE(__ret, __func, __t1, __p1, __t2, __p2, __t3, __p3, __t4, __p4, __t5, __p5, __t6, __p6, __t7, __p7, __t8, __p8, __t9, __p9, __t10, __p10, __t11, __p11, __t12, __p12, __t13, __p13, __t14, __p14, __t15, __p15)

#endif // BUILTIN_EXTENSIONS

#endif // ENTERPRISE_FUNCTION_CALL_H
