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

pcopy->bundletype = strdup(pp->bundletype);
pcopy->audit = pp->audit;
pcopy->lineno = pp->lineno;
pcopy->petype = pp->petype;      /* rtype of promisee - list or scalar recipient? */
pcopy->bundle = strdup(pp->bundle);
pcopy->ref = pp->ref;
pcopy->ref_alloc = pp->ref_alloc;
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
   struct Body *bp = NULL;
   struct FnCall *fp = NULL;
   struct Rlist *rp,*rnew;
   enum cfdatatype dtype;
   char *bodyname = NULL;

   /* A body template reference could look like a scalar or fn to the parser w/w () */

   switch (cp->type)
      {
      case CF_SCALAR:
          bodyname = (char *)cp->rval;
          if (cp->isbody)
             {
             bp = IsBody(BODIES,bodyname);
             }
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
         CfOut(cf_error,"","Body type mismatch for body reference \"%s\" in promise at line %d of %s (%s != %s)\n",bodyname,pp->lineno,(pp->audit)->filename,bp->type,cp->lval);
         ERRORCOUNT++;
         }
      
      /* Keep the referent body type as a boolean for convenience when checking later */
      
      AppendConstraint(&(pcopy->conlist),cp->lval,strdup("true"),CF_SCALAR,cp->classes,false);

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
            AppendConstraint(&(pcopy->conlist),scp->lval,returnval.item,returnval.rtype,scp->classes,false);
            }

         DeleteScope("body");
         }
      else
         {
         /* No arguments to deal with or body undeclared */

         if (fp != NULL)
            {
            CfOut(cf_error,"","An apparent body \"%s()\" was undeclared or could have incorrect args, but used in a promise near line %d of %s (possible unquoted literal value)",bodyname,pp->lineno,(pp->audit)->filename);
            }
         else
            {
            for (scp = bp->conlist; scp != NULL; scp = scp->next)
               {
               Debug("Doing sublval = %s (promises.c)\n",scp->lval);
               rnew = CopyRvalItem(scp->rval,scp->type);
               AppendConstraint(&(pcopy->conlist),scp->lval,rnew,scp->type,scp->classes,false);
               }
            }
         }
      }
   else
      {
      if (cp->isbody && !IsBundle(BUNDLES,bodyname))
         {
         CfOut(cf_error,"","Apparent body \"%s()\" was undeclared, but used in a promise near line %d of %s (possible unquoted literal value)",bodyname,pp->lineno,(pp->audit)->filename);
         }
      
      rnew = CopyRvalItem(cp->rval,cp->type);
      scp = AppendConstraint(&(pcopy->conlist),cp->lval,rnew,cp->type,cp->classes,false);
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

pcopy->bundletype = strdup(pp->bundletype);
pcopy->done = pp->done;
pcopy->donep = pp->donep;
pcopy->audit = pp->audit;
pcopy->lineno = pp->lineno;
pcopy->bundle = strdup(pp->bundle);
pcopy->ref = pp->ref;
pcopy->ref_alloc = pp->ref_alloc;
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
      DeleteRvalItem(returnval.item,returnval.rtype);
      }

   AppendConstraint(&(pcopy->conlist),cp->lval,final.item,final.rtype,cp->classes,false);

   if (strcmp(cp->lval,"comment") == 0)
      {
      if (final.rtype != CF_SCALAR)
         {
         yyerror("Comments can only be scalar objects");
         }
      else
         {
         pcopy->ref = final.item; /* No alloc reference to comment item */
         
         if (pcopy->ref && strstr(pcopy->ref,"$(this.promiser)"))
            {
            DereferenceComment(pcopy);
            }
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

pcopy->bundletype = strdup(pp->bundletype);
pcopy->done = pp->done;
pcopy->donep = pp->donep;
pcopy->audit = pp->audit;
pcopy->lineno = pp->lineno;
pcopy->bundle = strdup(pp->bundle);
pcopy->ref = pp->ref;
pcopy->ref_alloc = pp->ref_alloc;
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
   AppendConstraint(&(pcopy->conlist),cp->lval,final.item,final.rtype,cp->classes,false);

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

void DebugPromise(struct Promise *pp)

{ struct Constraint *cp;
  struct Body *bp;
  struct FnCall *fp;
  struct Rlist *rp;
  char *v,rettype,vbuff[CF_BUFSIZE];
  void *retval;
  time_t lastseen,last;
  double val,av,var;

if (GetVariable("control_common","version",&retval,&rettype) != cf_notype)
   {
   v = (char *)retval;
   }
else
   {
   v = "not specified";
   }

if (pp->promisee != NULL)
   {
   fprintf(stdout,"%s promise by \'%s\' -> ",pp->agentsubtype,pp->promiser);
   ShowRval(stdout,pp->promisee,pp->petype);
   fprintf(stdout," if context is %s\n",pp->classes);
   }
else
   {
   fprintf(stdout,"%s promise by \'%s\' (implicit) if context is %s\n",pp->agentsubtype,pp->promiser,pp->classes);
   }

fprintf(stdout,"in bundle %s of type %s\n",pp->bundle,pp->bundletype);

for (cp = pp->conlist; cp != NULL; cp = cp->next)
   {
   fprintf(stdout,"%10s => ",cp->lval);

   switch (cp->type)
      {
      case CF_SCALAR:
          if (bp = IsBody(BODIES,(char *)cp->rval))
             {
             ShowBody(bp,15);
             }
          else
             {
             ShowRval(stdout,cp->rval,cp->type); /* literal */
             }
          break;

      case CF_LIST:
          
          rp = (struct Rlist *)cp->rval;
          ShowRlist(stdout,rp);
          break;

      case CF_FNCALL:
          fp = (struct FnCall *)cp->rval;

          if (bp = IsBody(BODIES,fp->name))
             {
             ShowBody(bp,15);
             }
          else
             {
             ShowRval(stdout,cp->rval,cp->type); /* literal */
             }
          break;

      default:
          printf("Unknown RHS type %c\n",cp->type);
      }
   
   if (cp->type != CF_FNCALL)
      {
      fprintf(stdout," if body context %s\n",cp->classes);
      }
     
   }
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
if (pp == NULL)
   {
   return;
   }

if (pp->this_server != NULL)
   {
   ThreadLock(cft_policy);
   free(pp->this_server);
   ThreadUnlock(cft_policy);
   }

if (pp->next != NULL)
   {
   DeletePromises(pp->next);
   }

if (pp->ref_alloc == 'y')
   {
   ThreadLock(cft_policy);
   free(pp->ref);
   ThreadUnlock(cft_policy);
   }

DeletePromise(pp);
}

/*****************************************************************************/

struct Promise *NewPromise(char *typename,char *promiser)

{ struct Promise *pp;

ThreadLock(cft_policy); 

if ((pp = (struct Promise *)malloc(sizeof(struct Promise))) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate Promise");
   FatalError("");
   }

pp->audit = AUDITPTR;
pp->lineno = 0;
pp->bundle =  strdup("internal_bundle");
pp->promiser = strdup(promiser);

ThreadUnlock(cft_policy);

pp->promisee = NULL;
pp->petype = CF_NOPROMISEE;
pp->classes = NULL;
pp->done = false;
pp->donep = &(pp->done);

pp->this_server = NULL;
pp->cache = NULL;
pp->conn = NULL;
pp->inode_cache = NULL;
pp->cache = NULL;

pp->bundletype = NULL;
pp->agentsubtype = typename;   /* cache this, not copy strdup(typename);*/
pp->ref = NULL;                /* cache a reference if given*/
pp->ref_alloc = 'n';
pp->next = NULL;


pp->conlist = NULL;  // this fn is used for internal promises only
AppendConstraint(&(pp->conlist), "handle", strdup("internal_promise"),CF_SCALAR,NULL,false);

return pp;
}

/*****************************************************************************/

void DeletePromise(struct Promise *pp)

{
if (pp == NULL)
   {
   return;
   }

Debug("DeletePromise(%s->[%c])\n",pp->promiser,pp->petype);

ThreadLock(cft_policy);

if (pp->promiser != NULL)
   {
   free(pp->promiser);
   }

if (pp->promisee != NULL)
   {
   DeleteRvalItem(pp->promisee,pp->petype);
   }

free(pp->bundle);
free(pp->bundletype);
free(pp->classes);

// ref and agentsubtype are only references, do not free

DeleteConstraintList(pp->conlist);

free((char *)pp);
ThreadUnlock(cft_policy);
}

/*****************************************************************************/

void DeleteDeRefPromise(char *scopeid,struct Promise *pp)

{ struct Constraint *cp;

Debug("DeleteDeRefPromise()\n");

free(pp->promiser);

if (pp->promisee)
   {
   DeleteRvalItem(pp->promisee,pp->petype);
   }

if (pp->classes)
   {
   free(pp->classes);
   }

free(pp->bundle);

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

if (pp == NULL)
   {
   return;
   }

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

/*******************************************************************/

void HashPromise(char *salt,struct Promise *pp,unsigned char digest[EVP_MAX_MD_SIZE+1],enum cfhashes type)

{ EVP_MD_CTX context;
  int len, md_len;
  const EVP_MD *md = NULL;
  struct Constraint *cp;
  struct Rlist *rp;
  struct FnCall *fp;

  char *noRvalHash[] = { "mtime", "atime", "ctime", NULL };
  int doHash;
  int i;

md = EVP_get_digestbyname(FileHashName(type));
   
EVP_DigestInit(&context,md);

// multiple packages (promisers) may share same package_list_update_ifelapsed lock
if (!(salt && (strncmp(salt, PACK_UPIFELAPSED_SALT, sizeof(PACK_UPIFELAPSED_SALT) - 1) == 0)))
   {
   EVP_DigestUpdate(&context,pp->promiser,strlen(pp->promiser));
   }

if (pp->ref)
   {
   EVP_DigestUpdate(&context,pp->ref,strlen(pp->ref));
   }

if (pp->this_server)
   {
   EVP_DigestUpdate(&context,pp->this_server,strlen(pp->this_server));
   }

if (salt)
   {
   EVP_DigestUpdate(&context,salt,strlen(salt));
   }

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   EVP_DigestUpdate(&context,cp->lval,strlen(cp->lval));

   // don't hash rvals that change (e.g. times)
   doHash = true;

   for (i = 0; noRvalHash[i] != NULL; i++ )
      {
      if (strcmp(cp->lval, noRvalHash[i]) == 0)
	 {
         doHash = false;
         break;
	 }
      }
   
   if (!doHash)
      {
      continue;
      }
   
   switch(cp->type)
      {
      case CF_SCALAR:
          EVP_DigestUpdate(&context,cp->rval,strlen(cp->rval));
          break;

      case CF_LIST:
          for (rp = cp->rval; rp != NULL; rp=rp->next)
             {
             EVP_DigestUpdate(&context,rp->item,strlen(rp->item));
             }
          break;

      case CF_FNCALL:

          /* Body or bundle */

          fp = (struct FnCall *)cp->rval;

          EVP_DigestUpdate(&context,fp->name,strlen(fp->name));
          
          for (rp = fp->args; rp != NULL; rp=rp->next)
             {
             EVP_DigestUpdate(&context,rp->item,strlen(rp->item));
             }
          break;
      }
   }

EVP_DigestFinal(&context,digest,&md_len);
   
/* Digest length stored in md_len */
}

/*******************************************************************/

void DereferenceComment(struct Promise *pp)

{ char pre_buffer[CF_BUFSIZE],post_buffer[CF_BUFSIZE],buffer[CF_BUFSIZE],*sp;
  int offset = 0;

strncpy(pre_buffer,pp->ref,CF_BUFSIZE);

if (sp = strstr(pre_buffer,"$(this.promiser)"))
   {
   *sp = '\0';
   offset = sp - pre_buffer + strlen("$(this.promiser)");
   strncpy(post_buffer,pp->ref+offset,CF_BUFSIZE);
   snprintf(buffer,CF_BUFSIZE,"%s%s%s",pre_buffer,pp->promiser,post_buffer);

   if (pp->ref_alloc == 'y')
      {
      free(pp->ref);
      }
 
   pp->ref = strdup(buffer);
   pp->ref_alloc = 'y';
   }
}


