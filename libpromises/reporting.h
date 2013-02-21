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

#ifndef CFENGINE_REPORTING_H
#define CFENGINE_REPORTING_H

#include "cf3.defs.h"

#include "sequence.h"
#include "writer.h"

struct ReportContext_
{
    Writer *report_writers[REPORT_OUTPUT_TYPE_MAX];
};

ReportContext *ReportContextNew(void);
bool ReportContextAddWriter(ReportContext *context, ReportOutputType type, Writer *writer);
void ReportContextDestroy(ReportContext *context);

void ShowPromises(const ReportContext *context, const Seq *bundles, const Seq *bodies);
void ShowPromise(const ReportContext *context, const Promise *pp, int indent);
void ShowScopedVariables(const ReportContext *context, ReportOutputType type);
void ShowContext(const ReportContext *report_context);

// stdout only
void ReportError(char *s);
void BannerSubType(const char *bundlename, const char *type, int p);
void BannerSubSubType(const char *bundlename, const char *type);
void Banner(const char *s);

#endif
