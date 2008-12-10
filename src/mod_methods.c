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
/* File: mod_methods.c                                                       */
/*                                                                           */
/*****************************************************************************/

#define CF3_MOD_METHODS

#include "cf3.defs.h"
#include "cf3.extern.h"

/***************************************************************/

/* This is the primary set of constraints for a methods object */

struct BodySyntax CF_METHOD_BODIES[] =
   {
   {"usebundle",cf_bundle,CF_BUNDLE,"Specify the name of a bundle to run as a parameterized method"},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_METHOD_SUBTYPES[] =
  {
  {"agent","methods",CF_METHOD_BODIES},
  {NULL,NULL,NULL},
  };

