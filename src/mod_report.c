/* 
   Copyright (C) 2008 - Mark Burgess

   This file is part of Cfengine 3 - written and maintained by Mark Burgess.
 
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
   {"number_of_lines",cf_int,CF_VALRANGE},
   {"file_to_print",cf_str,CF_PATHRANGE},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/

/* This is the primary set of constraints for a file object */

struct BodySyntax CF_REPORT_BODIES[] =
   {
   {"lastseen",cf_int,CF_VALRANGE},
   {"intermittency",cf_real,"0,1"},
   {"showstate",cf_slist,""},
   {"printfile",cf_body,CF_PRINTFILE_BODY},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_REPORT_SUBTYPES[] =
  {

  /* Body lists belonging to "files:" type in Agent */
      
  {"agent","reports",CF_REPORT_BODIES},
  {NULL,NULL,NULL},
  };

