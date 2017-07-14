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

#ifndef CFENGINE_ITERATION_H
#define CFENGINE_ITERATION_H

#include <cf3.defs.h>

#include <rlist.h>
#include <variable.h>

typedef struct PromiseIterator_ PromiseIterator;

Rlist *ContainerToRlist(const JsonElement *container);
PromiseIterator *PromiseIteratorNew(EvalContext *ctx, const Promise *pp, const Rlist *lists, const Rlist *containers);
void PromiseIteratorDestroy(PromiseIterator *iter_ctx);

bool PromiseIteratorHasMore(const PromiseIterator *iter_ctx);
bool PromiseIteratorNext(PromiseIterator *iter_ctx);

bool NullIterators(const PromiseIterator *iter_ctx);

void PromiseIteratorUpdateVariable(EvalContext *ctx, const PromiseIterator *iter_ctx);

#endif
