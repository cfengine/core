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

/*****************************************************************************/
/*                                                                           */
/* File: mod_databases.c                                                     */
/*                                                                           */
/*****************************************************************************/

/*

 This file can act as a template for adding functionality to cfengine 3.
 All functionality can be added by extending the main array

 CF_MOD_SUBTYPES[CF3_MODULES]

 and its array dimension, in mod_common, in the manner shown here.
 
*/

#define CF3_MOD_DATABASES

#include "cf3.defs.h"
#include "cf3.extern.h"

 /***********************************************************/
 /* Read this module file backwards, as dependencies have   */
 /* to be defined first - these arrays declare pairs of     */
 /* constraints                                             */
 /*                                                         */
 /* lval => rval                                            */
 /*                                                         */
 /* in the form (lval,type,range)                           */
 /*                                                         */
 /* If the type is cf_body then the range is a pointer      */
 /* to another array of pairs, like in a body "sub-routine" */
 /*                                                         */
 /***********************************************************/

struct BodySyntax CF_SQLSERVER_BODY[] =
   {
   {"db_server_owner",cf_str,"","User name for database connection"},
   {"db_server_password",cf_str,"","Clear text password for database connection"},
   {"db_server_host",cf_str,"","Hostname or address for connection to database, blank means localhost"},
   {"db_server_type",cf_opts,"postgres,mysql","The dialect of the database server"},
   {"db_server_connection_db",cf_str,"","The name of an existing database to connect to in order to create/manage other databases"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/

struct BodySyntax CF_DATABASES_BODIES[] =
   {
   {"database_server",cf_body,CF_SQLSERVER_BODY,"Credentials for connecting to a local/remote database server"},
   {"database_type",cf_opts,"sql,ms_registry","The type of database that is to be manipulated"},
   {"database_operation",cf_opts,"create,delete,drop,cache,verify,restore","The nature of the promise - to be or not to be"},
   {"database_columns",cf_slist,".*","A list of column definitions to be promised by SQL databases"},
   {"database_rows",cf_slist,".*,.*","An ordered list of row values to be promised by SQL databases"},
   {"registry_exclude",cf_slist,"","A list of regular expressions to ignore in key/value verification"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_DATABASES_SUBTYPES[] =
  {
  {"agent","databases",CF_DATABASES_BODIES},
  {NULL,NULL,NULL},
  };

