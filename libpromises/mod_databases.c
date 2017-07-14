/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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

#include <mod_databases.h>

#include <syntax.h>

static const ConstraintSyntax database_server_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewString("db_server_owner", "", "User name for database connection", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("db_server_password", "", "Clear text password for database connection", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("db_server_host", "", "Hostname or address for connection to database, blank means localhost", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("db_server_type", "postgres,mysql", "The dialect of the database server. Default value: none", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("db_server_connection_db", "", "The name of an existing database to connect to in order to create/manage other databases", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax database_server_body = BodySyntaxNew("database_server", database_server_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax databases_constraints[] =
{
    ConstraintSyntaxNewBody("database_server", &database_server_body, "Credentials for connecting to a local/remote database server", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("database_type", "sql,ms_registry", "The type of database that is to be manipulated. Default value: none", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("database_operation", "create,delete,drop,cache,verify,restore", "The nature of the promise - to be or not to be", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("database_columns", ".*", "A list of column definitions to be promised by SQL databases", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("database_rows", ".*,.*", "An ordered list of row values to be promised by SQL databases", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("registry_exclude", "", "A list of regular expressions to ignore in key/value verification", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_DATABASES_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "databases", databases_constraints, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
