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

#ifndef CFENGINE3_ASSOC_H
#define CFENGINE3_ASSOC_H

#include "cf3.defs.h"

typedef struct CfAssoc        /* variable reference linkage , with metatype*/
   {
   char *lval;
   struct Rval rval;
   enum cfdatatype dtype;
   } CfAssoc;

struct CfAssoc *NewAssoc(const char *lval, struct Rval rval, enum cfdatatype dt);
void DeleteAssoc(struct CfAssoc *ap);
struct CfAssoc *CopyAssoc(struct CfAssoc *old);
struct CfAssoc *AssocNewReference(const char *lval, struct Rval rval, enum cfdatatype dtype);

#endif
