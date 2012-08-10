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

char *BodyName(const Promise *pp);
Body *IsBody(Body *list, const char *namespace, const char *key);
Bundle *IsBundle(Bundle *list, const char *key);
Promise *DeRefCopyPromise(const char *scopeid, const Promise *pp);
Promise *ExpandDeRefPromise(const char *scopeid, Promise *pp);
void DeletePromise(Promise *pp);
void DeletePromises(Promise *pp);
void PromiseRef(enum cfreport level, const Promise *pp);
Promise *NewPromise(char *type, char *promiser);
void HashPromise(char *salt, Promise *pp, unsigned char digest[EVP_MAX_MD_SIZE + 1], enum cfhashes type);

#endif
