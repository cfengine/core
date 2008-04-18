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
/* File: cf3globals.c                                                        */
/*                                                                           */
/* Created: Thu Aug  2 11:08:10 2007                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"

/*****************************************************************************/
/* operational state                                                         */
/*****************************************************************************/

FILE *FOUT = NULL;
short XML = false;
struct FnCallStatus FNCALL_STATUS;

/*****************************************************************************/
/* Internal data structures                                                  */
/*****************************************************************************/

struct PromiseParser P;
struct Bundle *BUNDLES = NULL;
struct Body *BODIES = NULL;
struct Scope *VSCOPE = NULL;
struct Rlist *VINPUTLIST = NULL;
struct Rlist *BODYPARTS = NULL;

/*****************************************************************************/
/* Constants                                                                 */
/*****************************************************************************/

struct SubTypeSyntax CF_NOSTYPE = {NULL,NULL,NULL};

/*****************************************************************************/

char *CF_DATATYPES[] = /* see enum cfdatatype */
   {
   "string",
   "int",
   "real",
   "slist",
   "ilist",
   "rlist",
   "(option)",
   "(option list)",
   "(ext body)",
   "class",
   "clist",
   "irange [int,int]",
   "rrange [real,real]",
   "<notype>",
   };

/*****************************************************************************/

char *CF_AGENTTYPES[] = /* see enum cfagenttype */
   {
   "*",
   "agent",
   "server",
   "monitor",
   "exec",
   "runagent",
   "<notype>",
   };
