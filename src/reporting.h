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

void ShowPromises(const Bundle *bundles, const Body *bodies);
void ShowPromise(Promise *pp, int indent);
void ShowScopedVariables(void);
void ShowPromisesInReport(const Bundle *bundles, const Body *bodies);
void ShowPromiseInReport(const char *version, const Promise *pp, int indent);

// stdout only
void SyntaxTree(void);
void ReportError(char *s);
void ShowContext(void);
void BannerSubType(const char *bundlename, const char *type, int p);
void BannerSubSubType(const char *bundlename, const char *type);
void Banner(const char *s);

#endif
