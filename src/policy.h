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

#ifndef CFENGINE_POLICY_H
#define CFENGINE_POLICY_H

#include "cf3.defs.h"
#include "sequence.h"

struct Policy_
{
    Bundle *bundles;
    Body *bodies;
    char *current_namespace;
};

Policy *PolicyNew(void);
void PolicyDestroy(Policy *policy);

Policy *PolicyFromPromise(const Promise *promise);
char *BundleQualifiedName(const Bundle *bundle);

typedef enum
{
    POLICY_ELEMENT_TYPE_BUNDLE,
    POLICY_ELEMENT_TYPE_BODY,
    POLICY_ELEMENT_TYPE_SUBTYPE,
    POLICY_ELEMENT_TYPE_PROMISE,
    POLICY_ELEMENT_TYPE_CONSTRAINT
} PolicyElementType;

typedef struct
{
    PolicyElementType type;
    const void *subject;
    char *message;
} PolicyError;

PolicyError *PolicyErrorNew(PolicyElementType type, const void *subject, const char *error_msg, ...);
void PolicyErrorDestroy(PolicyError *error);
void PolicyErrorWrite(Writer *writer, const PolicyError *error);
bool PolicyCheck(const Policy *policy, Sequence *errors);
void PolicySetNameSpace(Policy *policy, char *namespace);
char *CurrentNameSpace(Policy *policy);

#endif
