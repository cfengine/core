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

struct BodySyntax CF_RELATE_BODY[] =
   {
   {"forward_relationship",cf_str,"","Name of forward association between promiser topic and associates"},
   {"backward_relationship",cf_str,"","Name of backward/inverse association from associates to promiser topic"},
   {"associates",cf_slist,"","List of associated topics by this forward relationship"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/

struct BodySyntax CF_OCCUR_BODIES[] =
   {
   {"represents",cf_slist,"","List of subtopics that explains the type(s) of information represented by the occurrence"},
   {"representation",cf_opts,"literal,url,db,file,web,image,portal","How to interpret the promiser string e.g. actual data or reference to data"},
   {"web_root",cf_str,"","Base URL of the occurrence when rendered as a web-URL (replaces path_root)"},
   {"path_root",cf_str,"","Base path of the occurrence when locating file (replaced by web_root)"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/

/* This is the primary set of constraints for a file object */

struct BodySyntax CF_TOPICS_BODIES[] =
   {
   {"association",cf_body,CF_RELATE_BODY,"Declare associated topics"},
   {"comment",cf_str,"","Retained comment about this promise's real intention"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_KNOWLEDGE_SUBTYPES[] =
  {
  {"knowledge","topics",CF_TOPICS_BODIES},
  {"knowledge","occurrences",CF_OCCUR_BODIES},
  {NULL,NULL,NULL},
  };

