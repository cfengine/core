/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_PROMISE_LOGGING_H
#define CFENGINE_PROMISE_LOGGING_H

/*
 * This module provides a way to adjust logging levels while evaluating
 * promises.
 */

#include "cf3.defs.h"
#include "policy.h"

/**
 * @brief Binds logging in current thread to EvalContext.
 */
void PromiseLoggingInit(const EvalContext *ctx);

/**
 * @brief Calculates and sets logging context for the promise.
 */
void PromiseLoggingPromiseEnter(const EvalContext *ctx, const Promise *pp);

/**
 * @brief Finishes processing the promise and looks up the last error message associated with it.
 *
 * @return Last log message recorded for the promise, or NULL if there were none. Caller owns the memory.
 */
char *PromiseLoggingPromiseFinish(const EvalContext *ctx, const Promise *pp);

/**
 * @brief Unbinds logging from EvalContext.
 */
void PromiseLoggingFinish(const EvalContext *ctx);

#endif
