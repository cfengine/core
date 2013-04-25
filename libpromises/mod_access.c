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

  CF_MOD_PROMISE_TYPES[CF3_MODULES]

  and its array dimension, in mod_common, in the manner shown here.
*/

#include "mod_access.h"

#include "syntax.h"

/*
  Read this module file backwards, as dependencies have to be defined first -
  these arrays declare pairs of constraints

  lval => rval

  in the form (lval,type,range)

  If the type is cf_body then the range is a pointer to another array of pairs,
  like in a body "sub-routine"
*/

const ConstraintSyntax CF_REMACCESS_BODIES[] =
{
    ConstraintSyntaxNewStringList("admit", "", "List of host names or IP addresses to grant access to file objects", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("deny", "", "List of host names or IP addresses to deny access to file objects", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("maproot", "", "List of host names or IP addresses to grant full read-privilege on the server", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("ifencrypted", "true/false whether the current file access promise is conditional on the connection from the client being encrypted. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("resource_type", "path,literal,context,query,variable", "The type of object being granted access (the default grants access to files)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CF_REMROLE_BODIES[] =
{
    ConstraintSyntaxNewStringList("authorize", "", "List of public-key user names that are allowed to activate the promised class during remote agent activation", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_REMACCESS_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("server", "access", CF_REMACCESS_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("server", "roles", CF_REMROLE_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
