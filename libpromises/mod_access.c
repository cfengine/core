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

/*
  This file can act as a template for adding functionality to cfengine 3.  All
  functionality can be added by extending the main array

  CF_MOD_SUBTYPES[CF3_MODULES]

  and its array dimension, in mod_common, in the manner shown here.
*/

#include "cf3.defs.h"
#include "mod_access.h"

/*
  Read this module file backwards, as dependencies have to be defined first -
  these arrays declare pairs of constraints

  lval => rval

  in the form (lval,type,range)

  If the type is cf_body then the range is a pointer to another array of pairs,
  like in a body "sub-routine"
*/

const BodySyntax CF_REMACCESS_BODIES[] =
{
    {"admit", DATA_TYPE_STRING_LIST, "", "List of host names or IP addresses to grant access to file objects"},
    {"deny", DATA_TYPE_STRING_LIST, "", "List of host names or IP addresses to deny access to file objects"},
    {"maproot", DATA_TYPE_STRING_LIST, "", "List of host names or IP addresses to grant full read-privilege on the server"},
    {"ifencrypted", DATA_TYPE_OPTION, CF_BOOL,
     "true/false whether the current file access promise is conditional on the connection from the client being encrypted",
     "false"},
    {"resource_type", DATA_TYPE_OPTION, "path,literal,context,query,variable",
     "The type of object being granted access (the default grants access to files)"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const BodySyntax CF_REMROLE_BODIES[] =
{
    {"authorize", DATA_TYPE_STRING_LIST, "",
     "List of public-key user names that are allowed to activate the promised class during remote agent activation"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const SubTypeSyntax CF_REMACCESS_SUBTYPES[] =
{
    {"server", "access", CF_REMACCESS_BODIES},
    {"server", "roles", CF_REMROLE_BODIES},
    {NULL, NULL, NULL},
};
