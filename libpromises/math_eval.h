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

#ifndef CFENGINE_MATH_EVAL_H
#define CFENGINE_MATH_EVAL_H

#include <cf3.defs.h>

double EvaluateMathInfix(EvalContext *ctx, const char *input, char *failure);
double EvaluateMathFunction(const char *f, double p);
double _math_eval_step(double p);

static char *math_eval_function_names[] = 
{
    "ceil", "floor", "log10", "log2", "log", "sqrt", "sin", "cos", "tan", "asin", "acos", "atan", "abs", "step"
};

static double (*math_eval_functions[]) (double) = 
{
    ceil, floor, log10, log2, log, sqrt, sin, cos, tan, asin, acos, atan, fabs, _math_eval_step
};

#endif // MATH_EVAL_H
