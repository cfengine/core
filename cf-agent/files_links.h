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

#ifndef CFENGINE_FILES_LINKS_H
#define CFENGINE_FILES_LINKS_H

#include <cf3.defs.h>

PromiseResult VerifyLink(EvalContext *ctx, char *destination, const char *source, Attributes attr, const Promise *pp);
PromiseResult VerifyAbsoluteLink(EvalContext *ctx, char *destination, const char *source, Attributes attr, const Promise *pp);
PromiseResult VerifyRelativeLink(EvalContext *ctx, char *destination, const char *source, Attributes attr, const Promise *pp);
PromiseResult VerifyHardLink(EvalContext *ctx, char *destination, const char *source, Attributes attr, const Promise *pp);
bool KillGhostLink(EvalContext *ctx, const char *name, Attributes attr, const const Promise *pp, PromiseResult *result);
bool MakeHardLink(EvalContext *ctx, const char *from, const char *to, Attributes attr, const Promise *pp, PromiseResult *result);
int ExpandLinks(char *dest, const char *from, int level);

#endif
