/* 

        Copyright (C) 1994-
        Free Software Foundation, Inc.

   This file is part of GNU cfengine - written and maintained 
   by Mark Burgess, Dept of Computing and Engineering, Oslo College,
   Dept. of Theoretical physics, University of Oslo
 
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
/* File: mod_knowledge.c                                                     */
/*                                                                           */
/*****************************************************************************/

#define CF3_MOD_KNOWLEDGE

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

struct BodySyntax CF_OCCUR_BODIES[] =
   {
   {"mentions",cf_slist,""},
   {"usecontent",cf_slist,CF_PATHRANGE},
   {"stylesheet",cf_str,""},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/

/* This is the primary set of constraints for a file object */

struct BodySyntax CF_TOPICS_BODIES[] =
   {
   {"association",cf_str,""},
   {"occursin",cf_slist,""},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_KNOWLEDGE_SUBTYPES[] =
  {

  /* Body lists belonging to "files:" type in Agent */
      
  {"knowledge","topics",CF_TOPICS_BODIES},
  {"knowledge","occurrences",CF_OCCUR_BODIES},
  };

