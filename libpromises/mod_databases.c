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

#include "mod_databases.h"

#include "syntax.h"

static const ConstraintSyntax CF_SQLSERVER_BODY[] =
{
    ConstraintSyntaxNewString("db_server_owner", "", "User name for database connection", NULL),
    ConstraintSyntaxNewString("db_server_password", "", "Clear text password for database connection", NULL),
    ConstraintSyntaxNewString("db_server_host", "", "Hostname or address for connection to database, blank means localhost", NULL),
    ConstraintSyntaxNewOption("db_server_type", "postgres,mysql", "The dialect of the database server", "none"),
    ConstraintSyntaxNewString("db_server_connection_db", "",
     "The name of an existing database to connect to in order to create/manage other databases", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_DATABASES_BODIES[] =
{
    ConstraintSyntaxNewBody("database_server", CF_SQLSERVER_BODY, "Credentials for connecting to a local/remote database server"),
    ConstraintSyntaxNewOption("database_type", "sql,ms_registry", "The type of database that is to be manipulated", "none"),
    ConstraintSyntaxNewOption("database_operation", "create,delete,drop,cache,verify,restore", "The nature of the promise - to be or not to be", NULL),
    ConstraintSyntaxNewStringList("database_columns", ".*", "A list of column definitions to be promised by SQL databases"),
    ConstraintSyntaxNewStringList("database_rows", ".*,.*", "An ordered list of row values to be promised by SQL databases"),
    ConstraintSyntaxNewStringList("registry_exclude", "", "A list of regular expressions to ignore in key/value verification"),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_DATABASES_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "databases", ConstraintSetSyntaxNew(CF_DATABASES_BODIES, NULL)),
    PromiseTypeSyntaxNewNull(),
};
