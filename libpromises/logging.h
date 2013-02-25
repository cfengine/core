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

#ifndef CFENGINE_LOGGING_H
#define CFENGINE_LOGGING_H

#include "cf3.defs.h"

void BeginAudit(void);
void EndAudit(int background_tasks);
void ClassAuditLog(EvalContext *ctx, const Promise *pp, Attributes attr, char status, char *reason);
void PromiseLog(char *s);
void PromiseBanner(EvalContext *ctx, Promise *pp);
void BannerSubBundle(Bundle *bp, Rlist *params);
void FatalError(char *s, ...) FUNC_ATTR_NORETURN FUNC_ATTR_PRINTF(1, 2);

#endif
