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
/* File: install.c                                                           */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*******************************************************************/

int RelevantBundle(char *agent,char *blocktype)

{ struct Item *ip;
 
if (strcmp(agent,CF_AGENTTYPES[cf_common]) == 0 || strcmp(CF_COMMONC,P.blocktype) == 0)
   {
   return true;
   }

/* Here are some additional bundle types handled by cfAgent */

ip = SplitString("edit_line,edit_xml",',');

if (strcmp(agent,CF_AGENTTYPES[cf_agent]) == 0)
   {
   if (IsItemIn(ip,blocktype))
      {
      DeleteItemList(ip);
      return true;
      }
   }

DeleteItemList(ip);
return false;
}

/*******************************************************************/

struct Bundle *AppendBundle(struct Bundle **start,char *name, char *type, struct Rlist *args)

{ struct Bundle *bp,*lp;
  char *sp;
  struct Rlist *rp;

if (INSTALL_SKIP)
   {
   return NULL;
   }
  
Debug("Appending new bundle %s %s (",type,name);

if (DEBUG)
   {
   ShowRlist(stdout,args);
   }
Debug(")\n");

CheckBundle(name,type);

if ((bp = (struct Bundle *)malloc(sizeof(struct Bundle))) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to alloc Bundle");
   FatalError("");
   }

if ((sp = strdup(name)) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate Bundle");
   FatalError("");
   }

if (*start == NULL)
   {
   *start = bp;
   }
else
   {
   for (lp = *start; lp->next != NULL; lp=lp->next)
      {
      }

   lp->next = bp;
   }

bp->name = sp;
bp->next = NULL;
bp->type = type;
bp->args = args;
bp->subtypes = NULL;
return bp;
}

/*******************************************************************/

struct Body *AppendBody(struct Body **start,char *name, char *type, struct Rlist *args)

{ struct Body *bp,*lp;
  char *sp;
  struct Rlist *rp;

Debug("Appending new promise body %s %s(",type,name);

CheckBody(name,type);

for (rp = args; rp!= NULL; rp=rp->next)
   {
   Debug("%s,",(char *)rp->item);
   }
Debug(")\n");

if ((bp = (struct Body *)malloc(sizeof(struct Body))) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate Body");
   FatalError("");
   }

if ((sp = strdup(name)) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate Body");
   FatalError("");
   }

if (*start == NULL)
   {
   *start = bp;
   }
else
   {
   for (lp = *start; lp->next != NULL; lp=lp->next)
      {
      }

   lp->next = bp;
   }

bp->name = sp;
bp->next = NULL;
bp->type = type;
bp->args = args;
bp->conlist = NULL;
return bp;
}

/*******************************************************************/

struct SubType *AppendSubType(struct Bundle *bundle,char *typename)

{ struct SubType *tp,*lp;
  char *sp;
  
if (INSTALL_SKIP)
   {
   return NULL;
   }

Debug("Appending new type section %s\n",typename);

if (bundle == NULL)
   {
   yyerror("Software error. Attempt to add a type without a bundle\n");
   FatalError("Stopped");
   }

for (lp = bundle->subtypes; lp != NULL; lp=lp->next)
   {
   if (strcmp(lp->name,typename) == 0)
      {
      return lp;
      }
   }

if ((tp = (struct SubType *)malloc(sizeof(struct SubType))) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate SubType");
   FatalError("");
   }

if ((sp = strdup(typename)) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate SubType");
   FatalError("");
   }

if (bundle->subtypes == NULL)
   {
   bundle->subtypes = tp;
   }
else
   {
   for (lp = bundle->subtypes; lp->next != NULL; lp=lp->next)
      {
      }

   lp->next = tp;
   }

tp->promiselist = NULL;
tp->name = sp;
tp->next = NULL;
return tp;
}

/*******************************************************************/

struct Promise *AppendPromise(struct SubType *type,char *promiser, void *promisee,char petype,char *classes,char *bundle,char *bundletype)

{ struct Promise *pp,*lp;
  char *sp = NULL,*spe = NULL;

if (INSTALL_SKIP)
   {
   return NULL;
   }
 
if (type == NULL)
   {
   yyerror("Software error. Attempt to add a promise without a type\n");
   FatalError("Stopped");
   }

/* Check here for broken promises - or later with more info? */

Debug("Appending Promise from bundle %s %s if context %s\n",bundle,promiser,classes);

if ((pp = (struct Promise *)malloc(sizeof(struct Promise))) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate Promise");
   FatalError("");
   }

if ((sp = strdup(promiser)) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate Promise");
   FatalError("");
   }

if (strlen(classes) > 0)
   {
   if ((spe = strdup(classes)) == NULL)
      {
      CfOut(cf_error,"malloc","Unable to allocate Promise");
      FatalError("");
      }
   }
else
   {
   if ((spe = strdup("any")) == NULL)
      {
      CfOut(cf_error,"malloc","Unable to allocate Promise");
      FatalError("");
      }
   }

if ((strcmp(spe,"any") == 0) && (strcmp(type->name,"reports") == 0))
   {
   yyerror("reports promises may not be in class \'any\' - risk of a notification explosion");
   }
   
if (type->promiselist == NULL)
   {
   type->promiselist = pp;
   }
else
   {
   for (lp = type->promiselist; lp->next != NULL; lp=lp->next)
      {
      }

   lp->next = pp;
   }

pp->audit = AUDITPTR;
pp->lineno = P.line_no;
pp->bundle =  strdup(bundle);
pp->promiser = sp;
pp->promisee = promisee;  /* this is a list allocated separately */
pp->petype = petype;      /* rtype of promisee - list or scalar recipient? */
pp->classes = spe;
pp->conlist = NULL;
pp->done = false;
pp->donep = &(pp->done);

pp->this_server = NULL;
pp->cache = NULL;
pp->conn = NULL;
pp->inode_cache = NULL;

pp->bundletype = bundletype;   /* cache agent,common,server etc*/
pp->agentsubtype = type->name; /* Cache the typename */
pp->ref = NULL;                /* cache a reference if given*/

pp->next = NULL;
return pp;
}

/*******************************************************************/
/* Cleanup                                                         */
/*******************************************************************/

void DeleteBundles(struct Bundle *bp)

{
if (bp == NULL)
   {
   return;
   }
 
if (bp->next != NULL)
   {
   DeleteBundles(bp->next);
   }

if (bp->name != NULL)
   {
   free(bp->name);
   }

DeleteRlist(bp->args);
DeleteSubTypes(bp->subtypes);
}

/*******************************************************************/

void DeleteSubTypes(struct SubType *tp)

{
if (tp == NULL)
   {
   return;
   }
 
if (tp->next != NULL)
   {
   DeleteSubTypes(tp->next);
   }

DeletePromises(tp->promiselist);

if (tp->name != NULL)
   {
   free(tp->name);
   }
}

/*******************************************************************/

void DeleteBodies(struct Body *bp)

{
if (bp == NULL)
   {
   return;
   }
 
if (bp->next != NULL)
   {
   DeleteBodies(bp->next);
   }

if (bp->name != NULL)
   {
   free(bp->name);
   }

DeleteRlist(bp->args);
DeleteConstraintList(bp->conlist);
}
