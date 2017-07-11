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

#ifndef CFENGINE_MOD_ACCESS_H
#define CFENGINE_MOD_ACCESS_H

#include <cf3.defs.h>

typedef enum
{
    REMOTE_ACCESS_ADMIT,
    REMOTE_ACCESS_DENY,
    REMOTE_ACCESS_ADMITIPS,
    REMOTE_ACCESS_DENYIPS,
    REMOTE_ACCESS_ADMITHOSTNAMES,
    REMOTE_ACCESS_DENYHOSTNAMES,
    REMOTE_ACCESS_ADMITKEYS,
    REMOTE_ACCESS_DENYKEYS,
    REMOTE_ACCESS_MAPROOT,
    REMOTE_ACCESS_IFENCRYPTED,
    REMOTE_ACCESS_RESOURCE_TYPE,
    REMOTE_ACCESS_REPORT_DATA_SELECT,
    REMOTE_ACCESS_SHORTCUT,
    REMOTE_ACCESS_NONE
} RemoteAccess;

typedef enum
{
    REMOTE_ROLE_AUTHORIZE,
    REMOTE_ROLE_NONE
} RemoteRole;


extern const PromiseTypeSyntax CF_REMACCESS_PROMISE_TYPES[];
extern const ConstraintSyntax CF_REMACCESS_BODIES[REMOTE_ACCESS_NONE + 1];
extern const ConstraintSyntax CF_REMROLE_BODIES[REMOTE_ROLE_NONE + 1];

#endif
