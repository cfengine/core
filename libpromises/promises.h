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

#ifndef CFENGINE_PROMISES_H
#define CFENGINE_PROMISES_H

#include "cf3.defs.h"

#include "sequence.h"

Body *IsBody(Seq *bodies, const char *ns, const char *key);
Bundle *IsBundle(Seq *bundles, const char *key);
Promise *DeRefCopyPromise(const char *scopeid, const Promise *pp);
Promise *ExpandDeRefPromise(const char *scopeid, Promise *pp);
void PromiseRef(OutputLevel level, const Promise *pp);
Promise *NewPromise(char *type, char *promiser);
void HashPromise(char *salt, Promise *pp, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type);

#endif
