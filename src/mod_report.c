/* 
   Copyright (C) 2008 - Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version. 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

*/

/*****************************************************************************/
/*                                                                           */
/* File: mod_report.c                                                        */
/*                                                                           */
/*****************************************************************************/

#define CF3_MOD_REPORT

#include "cf3.defs.h"
#include "cf3.extern.h"

struct BodySyntax CF_PRINTFILE_BODY[] =
   {
   {"number_of_lines",cf_int,CF_VALRANGE,"Integer maximum number of lines to print from selected file"},
   {"file_to_print",cf_str,CF_PATHRANGE,"Path name to the file that is to be sent to standard output"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/

/* This is the primary set of constraints for a file object */

struct BodySyntax CF_REPORT_BODIES[] =
   {
   {"lastseen",cf_int,CF_VALRANGE,"Integer time threshold in hours since current peers were last seen, report absence"},
   {"intermittency",cf_real,"0,1","Real number threshold [0,1] of intermittency about current peers, report above"},
   {"showstate",cf_slist,"","List of services about which status reports should be reported to standard output"},
   {"printfile",cf_body,CF_PRINTFILE_BODY,"Quote part of a file to standard output"},
   {"friend_pattern",cf_str,"","Regular expression to keep selected hosts from the friends report list"},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_REPORT_SUBTYPES[] =
  {
  /* Body lists belonging to "reports:" type in Agent */
      
  {"agent","reports",CF_REPORT_BODIES},
  {NULL,NULL,NULL},
  };

