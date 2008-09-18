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
/* File: cf3globals.c                                                        */
/*                                                                           */
/* Created: Thu Aug  2 11:08:10 2007                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"

/*****************************************************************************/
/* flags                                                                     */
/*****************************************************************************/

short SHOWREPORTS = false;

/*****************************************************************************/
/* operational state                                                         */
/*****************************************************************************/

FILE *FOUT = NULL;
short XML = false;
struct FnCallStatus FNCALL_STATUS;

int CFA_MAXTHREADS = 10;

char THIS_AGENT[CF_MAXVARSIZE];
enum cfagenttype THIS_AGENT_TYPE;
short INSTALL_SKIP = false;
int FACILITY;
time_t PROMISETIME;

struct Rlist *SERVERLIST = NULL;
struct Item *PROCESSTABLE = NULL;

char HASHDB[CF_BUFSIZE];

/*****************************************************************************/
/* Internal data structures                                                  */
/*****************************************************************************/

struct PromiseParser P;
struct Bundle *BUNDLES = NULL;
struct Body *BODIES = NULL;
struct Scope *VSCOPE = NULL;
struct Rlist *VINPUTLIST = NULL;
struct Rlist *BODYPARTS = NULL;
struct Rlist *SUBBUNDLES = NULL;
struct Rlist *ACCESSLIST = NULL;

struct Rlist *SINGLE_COPY_LIST = NULL;
struct Rlist *AUTO_DEFINE_LIST = NULL;
struct Rlist *SINGLE_COPY_CACHE = NULL;

struct Rlist *CF_STCK = NULL;
int CF_STCKFRAME = 0;

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
   "(ext bundle)",
   "class",
   "clist",
   "irange [int,int]",
   "rrange [real,real]",
   "<notype>",
   };

/*****************************************************************************/

char *CF_AGENTTYPES[] = /* see enum cfagenttype */
   {
   CF_COMMONC,
   CF_AGENTC,
   CF_SERVERC,
   CF_MONITORC,
   CF_EXECC,
   CF_RUNC,
   CF_KNOWC,
   "<notype>",
   };

