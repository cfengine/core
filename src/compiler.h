#ifndef CFENGINE_COMPILER_H
#define CFENGINE_COMPILER_H

/* Compiler-specific options/defines */

#if defined(__GNUC__) && (__GNUC__ * 100 >= 3)
# define FUNC_ATTR_NORETURN  __attribute__((noreturn))
#else /* not gcc >= 3.0 */
# define FUNC_ATTR_NORETURN
#endif

#endif
