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
/* File: promises.c                                                          */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

char *BodyName(struct Promise *pp)

{ char *name,*sp;
  int i,size = 0;
  struct Constraint *cp;

/* Return a type template for the promise body for lock-type identification */
 
if ((name = malloc(CF_MAXVARSIZE)) == NULL)
   {
   FatalError("BodyName");
   }

sp = pp->agentsubtype;

if (size + strlen(sp) < CF_MAXVARSIZE-CF_BUFFERMARGIN)
   {
   strcpy(name,sp);
   strcat(name,".");
   size += strlen(sp);
   }

for (i = 0,cp = pp->conlist; (i < 5) && cp != NULL; i++,cp=cp->next)
   {
   if (strcmp(cp->lval,"args") == 0) /* Exception for args, by symmetry, for locking */
      {
      continue;
      }
   
   if (size + strlen(cp->lval) < CF_MAXVARSIZE-CF_BUFFERMARGIN)
      {
      strcat(name,cp->lval);
      strcat(name,".");
      size += strlen(cp->lval);
      }
   }

return name; 
}

/*****************************************************************************/

struct Promise *DeRefCopyPromise(char *scopeid,struct Promise *pp)

{ struct Promise *pcopy;
  struct Constraint *cp,*scp;
  char scope[CF_BUFSIZE],naked[CF_MAXVARSIZE];
  struct Rval returnval;

if (pp->promisee)
   {
   Debug("CopyPromise(%s->",pp->promiser);
   if (DEBUG)
      {
      ShowRval(stdout,pp->promisee,pp->petype);
      }
   Debug("\n");
   }
else
   {
   Debug("CopyPromise(%s->)\n",pp->promiser);
   }

if ((pcopy = (struct Promise *)malloc(sizeof(struct Promise))) == NULL)
   {
   CfOut(cf_error,"malloc","Promise allocation failure");
   FatalError("memory");
   }

if (pp->promiser)
   {
   pcopy->promiser = strdup(pp->promiser);
   }

if (pp->promisee)
   {
   pcopy->promisee = CopyRvalItem(pp->promisee,pp->petype);
   pcopy->petype = pp->petype;
   }
else
   {
   pcopy->promisee = NULL;
   }

if (pp->classes)
   {
   pcopy->classes  = strdup(pp->classes);
   }

if (pcopy->promiser == NULL || (pp->promisee != NULL && pcopy->promisee == NULL) || pcopy->classes == NULL)
   {
   CfOut(cf_error,"malloc","Promise detail allocation failure");
   FatalError("memory");
   }

pcopy->bundletype = pp->bundletype;
pcopy->audit = pp->audit;
pcopy->lineno = pp->lineno;
pcopy->petype = pp->petype;      /* rtype of promisee - list or scalar recipient? */
pcopy->bundle = strdup(pp->bundle);
pcopy->ref = pp->ref;
pcopy->agentsubtype = pp->agentsubtype;
pcopy->done = pp->done;
pcopy->conlist = NULL;
pcopy->next = NULL;
pcopy->cache = NULL;
pcopy->inode_cache = pp->inode_cache;
pcopy->this_server = pp->this_server;
pcopy->donep = pp->donep;
pcopy->conn = pp->conn;
pcopy->edcontext = pp->edcontext;

Debug("Copying promise constraints\n\n");

/* No further type checking should be necessary here, already done by CheckConstraintTypeMatch */

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   struct Body *bp;
   struct FnCall *fp;
   struct Rlist *rp,*rnew;
   enum cfdatatype dtype;
   char *bodyname = NULL;

   /* A body template reference could look like a scalar or fn to the parser w/w () */

   switch (cp->type)
      {
      case CF_SCALAR:
          bodyname = (char *)cp->rval;
          bp = IsBody(BODIES,bodyname);
          fp = NULL;
          break;
      case CF_FNCALL:
          fp = (struct FnCall *)cp->rval;
          bodyname = fp->name;
          bp = IsBody(BODIES,bodyname);
          break;
      default:
          bp = NULL;
          fp = NULL;
          bodyname = NULL;
          break;
      }

   /* First case is: we have a body template to expand lval = body(args), .. */
   
   if (bp != NULL) 
      {
      if (strcmp(bp->type,cp->lval) != 0)
         {
         CfOut(cf_error,"","Body type mismatch for body reference \"%s\" in promise at line %d of %s\n",bodyname,pp->lineno,(pp->audit)->filename);
         ERRORCOUNT++;
         }
      
      /* Keep the referent body type as a boolean for convenience when checking later */
      
      AppendConstraint(&(pcopy->conlist),cp->lval,strdup("true"),CF_SCALAR,cp->classes);

      Debug("Handling body-lval \"%s\"\n",cp->lval);
      
      if (bp->args != NULL)
         {
         /* There are arguments to insert*/
         
         if (fp == NULL || fp->args == NULL)
            {
            CfOut(cf_error,"","Argument mismatch for body reference \"%s\" in promise at line %d of %s\n",bodyname,pp->lineno,(pp->audit)->filename);
            }
         
         NewScope("body");

         if (fp && bp && fp->args && bp->args && !MapBodyArgs("body",fp->args,bp->args))
            {
            ERRORCOUNT++;            
            CfOut(cf_error,"","Number of arguments does not match for body reference \"%s\" in promise at line %d of %s\n",bodyname,pp->lineno,(pp->audit)->filename);
            }
      
         for (scp = bp->conlist; scp != NULL; scp = scp->next)
            {
            Debug("Doing arg-mapped sublval = %s (promises.c)\n",scp->lval);
            returnval = ExpandPrivateRval("body",scp->rval,scp->type);
            AppendConstraint(&(pcopy->conlist),scp->lval,returnval.item,returnval.rtype,scp->classes);
            }

         DeleteScope("body");
         }
      else
         {
         /* No arguments to deal with or body undeclared */

         if (fp != NULL)
            {
            CfOut(cf_error,"","body \"%s()\" was undeclared, but used in a promise near line %d of %s",bodyname,pp->lineno,(pp->audit)->filename);
            }
         else
            {
            for (scp = bp->conlist; scp != NULL; scp = scp->next)
               {
               Debug("Doing sublval = %s (promises.c)\n",scp->lval);
               rnew = CopyRvalItem(scp->rval,scp->type);
               AppendConstraint(&(pcopy->conlist),scp->lval,rnew,scp->type,scp->classes);
               }
            }
         }
      }
   else
      {
      rnew = CopyRvalItem(cp->rval,cp->type);
      scp = AppendConstraint(&(pcopy->conlist),cp->lval,rnew,cp->type,cp->classes);
      }
   }

return pcopy;
}


/*****************************************************************************/

struct Promise *ExpandDeRefPromise(char *scopeid,struct Promise *pp)

{ struct Promise *pcopy;
  struct Constraint *cp,*scp;
  char scope[CF_BUFSIZE],naked[CF_MAXVARSIZE];
  struct Rval returnval,final;

Debug("ExpandDerefPromise()\n");

if ((pcopy = (struct Promise *)malloc(sizeof(struct Promise))) == NULL)
   {
   CfOut(cf_error,"malloc","Promise allocation failure");
   FatalError("memory");
   }

returnval = ExpandPrivateRval("this",pp->promiser,CF_SCALAR);
pcopy->promiser = (char *)returnval.item;

if (pp->promisee)
   {
   returnval = EvaluateFinalRval(scopeid,pp->promisee,pp->petype,true,pp);
   pcopy->promisee = (struct Rlist *)returnval.item;
   pcopy->petype = returnval.rtype;
   }
else
   {
   pcopy->petype = CF_NOPROMISEE;
   pcopy->promisee = NULL;
   }

if (pp->classes)
   {
   pcopy->classes = strdup(pp->classes);
   }
else
   {
   pcopy->classes = strdup("any");
   }

if (pcopy->promiser == NULL || pcopy->classes == NULL)
   {
   CfOut(cf_error,"malloc","ExpandPromise detail allocation failure");
   FatalError("memory");
   }

pcopy->bundletype = pp->bundletype;
pcopy->done = pp->done;
pcopy->donep = pp->donep;
pcopy->audit = pp->audit;
pcopy->lineno = pp->lineno;
pcopy->bundle = strdup(pp->bundle);
pcopy->ref = pp->ref;
pcopy->agentsubtype = pp->agentsubtype;
pcopy->conlist = NULL;
pcopy->next = NULL;
pcopy->cache = pp->cache;
pcopy->inode_cache = pp->inode_cache;
pcopy->this_server = pp->this_server;
pcopy->conn = pp->conn;
pcopy->edcontext = pp->edcontext;

/* No further type checking should be necessary here, already done by CheckConstraintTypeMatch */

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   struct FnCall *fp;
   struct Rlist *rp;
   struct Rval returnval;
   char type;

   if (ExpectedDataType(cp->lval) == cf_bundle)
      {
      final = ExpandBundleReference(scopeid,cp->rval,cp->type);
      }
   else
      {
      returnval = EvaluateFinalRval(scopeid,cp->rval,cp->type,false,pp);   
      final = ExpandDanglers(scopeid,returnval,pp);
      }

   AppendConstraint(&(pcopy->conlist),cp->lval,final.item,final.rtype,cp->classes);

   if (strcmp(cp->lval,"comment") == 0)
      {
      if (final.rtype != CF_SCALAR)
         {
         yyerror("Comments can only be scalar objects");
         }
      else
         {
         pcopy->ref = final.item; /* No alloc reference to comment item */
         }
      }

   }

return pcopy;
}

/*****************************************************************************/

struct Promise *CopyPromise(char *scopeid,struct Promise *pp)

{ struct Promise *pcopy;
  struct Constraint *cp,*scp;
  char scope[CF_BUFSIZE],naked[CF_MAXVARSIZE];
  struct Rval returnval,final;

Debug("CopyPromise()\n");

if ((pcopy = (struct Promise *)malloc(sizeof(struct Promise))) == NULL)
   {
   CfOut(cf_error,"malloc","Promise allocation failure");
   FatalError("memory");
   }

pcopy->promiser = strdup(pp->promiser);

if (pp->promisee)
   {
   returnval = EvaluateFinalRval(scopeid,pp->promisee,pp->petype,true,pp);
   pcopy->promisee = (struct Rlist *)returnval.item;
   pcopy->petype = returnval.rtype;
   }
else
   {
   pcopy->petype = CF_NOPROMISEE;
   pcopy->promisee = NULL;
   }

if (pp->classes)
   {
   pcopy->classes = strdup(pp->classes);
   }
else
   {
   pcopy->classes = strdup("any");
   }

if (pcopy->promiser == NULL || pcopy->classes == NULL)
   {
   CfOut(cf_error,"malloc","ExpandPromise detail allocation failure");
   FatalError("memory");
   }

pcopy->bundletype = pp->bundletype;
pcopy->done = pp->done;
pcopy->donep = pp->donep;
pcopy->audit = pp->audit;
pcopy->lineno = pp->lineno;
pcopy->bundle = strdup(pp->bundle);
pcopy->ref = pp->ref;
pcopy->agentsubtype = pp->agentsubtype;
pcopy->conlist = NULL;
pcopy->next = NULL;
pcopy->cache = pp->cache;
pcopy->inode_cache = pp->inode_cache;
pcopy->this_server = pp->this_server;
pcopy->conn = pp->conn;
pcopy->edcontext = pp->edcontext;

/* No further type checking should be necessary here, already done by CheckConstraintTypeMatch */

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   struct FnCall *fp;
   struct Rlist *rp;
   struct Rval returnval;
   char type;

   if (ExpectedDataType(cp->lval) == cf_bundle)
      {
       /* sub-bundles do not expand here */
      returnval = ExpandPrivateRval(scopeid,cp->rval,cp->type);
      }
   else
      {
      returnval = EvaluateFinalRval(scopeid,cp->rval,cp->type,false,pp);   
      }

   final = ExpandDanglers(scopeid,returnval,pp);
   AppendConstraint(&(pcopy->conlist),cp->lval,final.item,final.rtype,cp->classes);

   if (strcmp(cp->lval,"comment") == 0)
      {
      if (final.rtype != CF_SCALAR)
         {
         yyerror("Comments can only be scalar objects");
         }
      else
         {
         pcopy->ref = final.item; /* No alloc reference to comment item */
         }
      }
   }

return pcopy;
}


/*******************************************************************/

struct Body *IsBody(struct Body *list,char *key)

{ struct Body *bp;

for (bp = list; bp != NULL; bp = bp->next)
   {
   if (strcmp(bp->name,key) == 0)
      {
      return bp;
      }
   }

return NULL;
}

/*******************************************************************/

struct Bundle *IsBundle(struct Bundle *list,char *key)

{ struct Bundle *bp;

for (bp = list; bp != NULL; bp = bp->next)
   {
   if (strcmp(bp->name,key) == 0)
      {
      return bp;
      }
   }

return NULL;
}

/*****************************************************************************/
/* Cleanup                                                                   */
/*****************************************************************************/

void DeletePromises(struct Promise *pp)

{
if (pp->this_server != NULL)
   {
   free(pp->this_server);
   }
 
if (pp->next != NULL)
   {
   DeletePromises(pp->next);
   }

free(pp->bundle);
DeletePromise(pp);
}

/*****************************************************************************/

struct Promise *NewPromise(char *typename,char *promiser)

{ struct Promise *pp;
 
if ((pp = (struct Promise *)malloc(sizeof(struct Promise))) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate Promise");
   FatalError("");
   }

pp->audit = AUDITPTR;
pp->lineno = 0;
pp->bundle =  strdup("independent");
pp->promiser = strdup(promiser);
pp->promisee = NULL;
pp->petype = CF_NOPROMISEE;
pp->classes = NULL;
pp->conlist = NULL;
pp->done = false;
pp->donep = &(pp->done);

pp->this_server = NULL;
pp->cache = NULL;
pp->conn = NULL;
pp->inode_cache = NULL;

pp->bundletype = NULL;
pp->agentsubtype = strdup(typename);
pp->ref = NULL;                /* cache a reference if given*/
pp->next = NULL;
return pp;
}

/*****************************************************************************/

void DeletePromise(struct Promise *pp)

{
Debug("DeletePromise(%s->[%c])\n",pp->promiser,pp->petype);

if (pp->promiser != NULL)
   {
   free(pp->promiser);
   }

if (pp->promisee != NULL)
   {
   DeleteRvalItem(pp->promisee,pp->petype);
   }

// ref/agentsubtype are only references

DeleteConstraintList(pp->conlist);
}

/*****************************************************************************/

void DeleteDeRefPromise(char *scopeid,struct Promise *pp)

{ struct Constraint *cp;

Debug("ExpandDerefPromise()\n");

free(pp->promiser);

if (pp->promisee)
   {
   DeleteRvalItem(pp->promisee,pp->petype);
   }

if (pp->classes)
   {
   free(pp->classes);
   }

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   free(cp->lval);
   DeleteRvalItem(cp->rval,cp->type);
   }

free(pp);
}

/*****************************************************************************/

void PromiseRef(enum cfreport level,struct Promise *pp)

{ char *v,rettype;
  void *retval;

if (GetVariable("control_common","version",&retval,&rettype) != cf_notype)
   {
   v = (char *)retval;
   }
else
   {
   v = "not specified";
   }

if (pp->audit)
   {
   CfOut(level,"","Promise (version %s) belongs to bundle \'%s\' in file \'%s\' near line %d\n",v,pp->bundle,pp->audit->filename,pp->lineno);
   }
else
   {
   CfOut(level,"","Promise (version %s) belongs to bundle \'%s\' near line %d\n",v,pp->bundle,pp->lineno);
   }

if (pp->ref)
   {
   CfOut(level,"","Comment: %s\n",pp->ref);
   }
}
