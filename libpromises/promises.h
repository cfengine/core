/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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

#ifndef CFENGINE_PROMISES_H
#define CFENGINE_PROMISES_H

#include <cf3.defs.h>

#include <logging.h>
#include <sequence.h>

Promise *DeRefCopyPromise(EvalContext *ctx, const Promise *pp);
Promise *ExpandDeRefPromise(EvalContext *ctx, const Promise *pp, bool *excluded);
void PromiseRef(LogLevel level, const Promise *pp);
void CopyBodyConstraintsToPromise(EvalContext *ctx, Promise *pp,
                                  const Body *bp);

#endif
