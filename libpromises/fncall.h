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

#ifndef CFENGINE_FNCALL_H
#define CFENGINE_FNCALL_H

#include <cf3.defs.h>

struct FnCall_
{
    char *name;
    Rlist *args;

    const Promise *caller;
};

typedef enum FnCallStatus
{
    FNCALL_SUCCESS,
    FNCALL_FAILURE
} FnCallStatus;

typedef struct
{
    FnCallStatus status;
    Rval rval;
} FnCallResult;

typedef struct
{
    const char *pattern;
    DataType dtype;
    const char *description;
} FnCallArg;

typedef enum
{
    FNCALL_OPTION_NONE = 0,
    FNCALL_OPTION_VARARG = 1 << 0,
    FNCALL_OPTION_CACHED = 1 << 1
} FnCallOption;

typedef struct
{
    const char *name;
    DataType dtype;
    const FnCallArg *args;
    FnCallResult (*impl)(EvalContext *, const Policy *, const FnCall *, const Rlist *);
    const char *description;
    FnCallOption options;
    FnCallCategory category;
    SyntaxStatus status;
} FnCallType;

extern const FnCallType CF_FNCALL_TYPES[];

bool FnCallIsBuiltIn(Rval rval);

FnCall *FnCallNew(const char *name, Rlist *args);
FnCall *FnCallCopy(const FnCall *f);
void FnCallDestroy(FnCall *fp);
unsigned FnCallHash(const FnCall *fp, unsigned seed, unsigned max);
void FnCallWrite(Writer *writer, const FnCall *call);


FnCallResult FnCallEvaluate(EvalContext *ctx, const Policy *policy, FnCall *fp, const Promise *caller);

const FnCallType *FnCallTypeGet(const char *name);

FnCall *ExpandFnCall(EvalContext *ctx, const char *ns, const char *scope, const FnCall *f);

// TODO: should probably demolish this eventually
void FnCallShow(FILE *fout, const char *prefix, const FnCall *fp, const Rlist *args);

#endif
