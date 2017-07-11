/* 
   Copyright 2017 Northern.tech AS

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <math_eval.h>
#include <compiler.h>

#define MATH_EVAL_STACK_SIZE 1024

const char *const math_eval_function_names[] =
{
    "ceil", "floor", "log10", "log2", "log", "sqrt", "sin", "cos", "tan", "asin", "acos", "atan", "abs", "step"
};

static double _math_eval_step(double p)
{
    return ((p < 0) ? 0 : 1);
}

typedef double (*MathEvalFunctionType)(double);

static const MathEvalFunctionType math_eval_functions[] =
{
    ceil, floor, log10, log2, log, sqrt, sin, cos, tan, asin, acos, atan, fabs, _math_eval_step
};


double math_eval_push(double n, double *stack, int *stackp)
{
    if (*stackp > MATH_EVAL_STACK_SIZE)
    {
        Log(LOG_LEVEL_ERR, "Math evaluation stack size exceeded");
        return 0;
    }

    return stack[++(*stackp)]= n; 
}

double math_eval_pop(double *stack, int *stackp)
{
    if (*stackp < 0)
    {
        Log(LOG_LEVEL_ERR, "Math evaluation stack could not be popped, internal error!");
        return 0;
    }

    return stack[(*stackp)--];
}

#define YYSTYPE double
#define YYPARSE yymath_parse
#define YYPARSEFROM yymath_parsefrom
#define YY_CTX_LOCAL
#define YY_PARSE(T) T
#define YY_INPUT(ctx, buf, result, max_size) {                     \
    result = 0;                                                    \
    if (NULL != ctx->input)                                        \
    {                                                              \
        /*Log(LOG_LEVEL_ERR, "YYINPUT: %s", ctx->input);*/         \
        strncpy(buf, ctx->input, max_size);                        \
        int n = strlen(ctx->input)+1;                              \
        if (n > max_size) n = max_size;                            \
        if (n > 0) buf[n - 1]= '\0';                               \
        result = strlen(buf);                                      \
        ctx->input = NULL;                                         \
    }                                                              \
    }

#undef malloc
#undef realloc
#define malloc xmalloc
#define realloc xrealloc

#define YY_CTX_MEMBERS char *failure;                                   \
    const char *input;                                                  \
    const char *original_input;                                         \
    EvalContext *eval_context;                                          \
    double result;                                                      \
    char fname[50];                                                     \
    double stack[MATH_EVAL_STACK_SIZE];                                 \
    int stackp;

/* Mark unused functions as such */
struct _yycontext;
static int yyAccept(struct _yycontext *yy, int tp0) FUNC_UNUSED;
static void yyPush(struct _yycontext *yy, char *text, int count) FUNC_UNUSED;
static void yyPop(struct _yycontext *yy, char *text, int count) FUNC_UNUSED;
static void yySet(struct _yycontext *yy, char *text, int count) FUNC_UNUSED;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <math.pc>

#pragma GCC diagnostic pop

double EvaluateMathInfix(EvalContext *ctx, const char *input, char *failure)
{
    yycontext yyctx;
    memset(&yyctx, 0, sizeof(yycontext));
    yyctx.failure = failure;
    yyctx.original_input = input;
    yyctx.input = input;
    yyctx.eval_context = ctx;
    yyctx.result = 0;
    yyctx.stackp = -1;
    yymath_parse(&yyctx);
    yyrelease(&yyctx);
    return yyctx.result;
}

double EvaluateMathFunction(const char *f, double p)
{
    int count = sizeof(math_eval_functions)/sizeof(math_eval_functions[0]);

    for (int i=0; i < count; i++)
    {
        if (0 == strcmp(math_eval_function_names[i], f))
        {
            return (*math_eval_functions[i])(p);
        }
    }

    return p;
}
