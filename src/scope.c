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
/* File: scope.c                                                             */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

static void DebugVariables(char *label);

/*******************************************************************/

static void DebugVariables(char *label)
/* 
 * Not thread safe 
 */
{ struct Scope *ptr;

 printf("----------------------%s ---------------------------\n",label);
 
for (ptr = VSCOPE; ptr != NULL; ptr=ptr->next)
   {
   printf("\nConstant variables in SCOPE %s:\n",ptr->scope);
   PrintHashes(stdout,ptr->hashtable,0);
   }

printf("--------------------------------------------------\n");
}

/*******************************************************************/

struct Scope *GetScope(char *scope)
/* 
 * Not thread safe - returns pointer to global memory
 */
{ struct Scope *cp = NULL;

Debug("Searching for scope context %s\n",scope);

for (cp = VSCOPE; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->scope,scope) == 0)
      {
      Debug("Found scope reference %s\n",scope);
      return cp;
      }
   }

return NULL;
}

/*******************************************************************/

void SetScope(char *id)

{
strncpy(CONTEXTID,id,31);
}

/*******************************************************************/

void SetNewScope(char *id)

{
NewScope(id);
strncpy(CONTEXTID,id,31);
}

/*******************************************************************/

void NewScope(char *name)
/*
 * Thread safe
 */
{ struct Scope *ptr;
  
Debug("Adding scope data %s\n", name);

if (!ThreadLock(cft_vscope))
   {
   CfOut(cf_error, "", "!! Could not lock VSCOPE");
   return;
   }
 
for (ptr = VSCOPE; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->scope,name) == 0)
      {
      ThreadUnlock(cft_vscope);
      Debug("SCOPE Object %s already exists\n",name);
      return;
      }
   }

if ((ptr = (struct Scope *)malloc(sizeof(struct Scope))) == NULL)
   {
   FatalError("Memory Allocation failed for Scope");
   }

InitHashes(ptr->hashtable);

ptr->next = VSCOPE;
ptr->scope = strdup(name);
VSCOPE = ptr;
ThreadUnlock(cft_vscope);
}

/*******************************************************************/

void AugmentScope(char *scope,struct Rlist *lvals,struct Rlist *rvals)

{ struct Scope *ptr;
  struct Rlist *rpl,*rpr;
  struct Rval retval;
  char *lval,naked[CF_BUFSIZE];
  HashIterator i;
  struct CfAssoc *assoc;

if (RlistLen(lvals) != RlistLen(rvals))
   {
   CfOut(cf_error,"","While constructing scope \"%s\"\n",scope);
   fprintf(stderr,"Formal = ");
   ShowRlist(stderr,lvals);
   fprintf(stderr,", Actual = ");
   ShowRlist(stderr,rvals);
   fprintf(stderr,"\n");
   FatalError("Augment scope, formal and actual parameter mismatch is fatal");
   }

for (rpl = lvals, rpr=rvals; rpl != NULL; rpl = rpl->next,rpr = rpr->next)
   {
   lval = (char *)rpl->item;

   CfOut(cf_verbose,"","    ? Augment scope %s with %s (%c)\n",scope,lval,rpr->type);

   // CheckBundleParameters() already checked that there is no namespace collision
   // By this stage all functions should have been expanded, so we only have scalars left

   if (IsNakedVar(rpr->item,'@'))
      {
      enum cfdatatype vtype;
      GetNaked(naked,rpr->item);

      vtype = GetVariable(scope,naked,&(retval.item),&(retval.rtype));

      switch(vtype)
         {
         case cf_slist:
         case cf_ilist:
         case cf_rlist:
             NewList(scope,lval,CopyRvalItem(retval.item,CF_LIST),cf_slist);
             break;
         default:
             CfOut(cf_error,""," !! List parameter \"%s\" not found while constructing scope \"%s\" - use @(scope.variable) in calling reference",naked,scope);
             NewScalar(scope,lval,rpr->item,cf_str);             
             break;
         }
      }
   else
      {
      NewScalar(scope,lval,rpr->item,cf_str);
      }
   }

/* Check that there are no danglers left to evaluate in the hash table itself */

ptr = GetScope(scope);

i = HashIteratorInit(ptr->hashtable);

while ((assoc = HashIteratorNext(&i)))
   {
   retval = ExpandPrivateRval(scope,(char *)(assoc->rval),assoc->rtype);
   // Retain the assoc, just replace rval
   DeleteRvalItem(assoc->rval,assoc->rtype);
   assoc->rval = retval.item;
   assoc->rtype = retval.rtype;
   }

return;
}

/*******************************************************************/

void DeleteAllScope()

{ struct Scope *ptr, *this;
  
Debug("Deleting all scoped variables\n");

if (!ThreadLock(cft_vscope))
   {
   CfOut(cf_error, "", "!! Could not lock VSCOPE");
   return;
   }
 
ptr = VSCOPE;

while (ptr != NULL)
   {
   this = ptr;
   Debug(" -> Deleting scope %s\n",ptr->scope);
   DeleteHashes(this->hashtable);
   free(this->scope);   
   ptr = this->next;
   free((char *)this);
   }

VSCOPE = NULL;

ThreadUnlock(cft_vscope);
}

/*******************************************************************/

void DeleteScope(char *name)
/*
 * Thread safe
 */

{ struct Scope *ptr, *prev = NULL;
  int found = false;
 
Debug1("Deleting scope %s\n", name);

if (!ThreadLock(cft_vscope))
   {
   CfOut(cf_error, "", "!! Could not lock VSCOPE");
   return;
   }


for (ptr = VSCOPE; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->scope,name) == 0)
      {
      Debug("Object %s exists\n",name);
      found = true;
      break;
      }
   else
      {
      prev = ptr;
      }
   }

if (!found)
   {
   Debug("No such scope to delete\n");
   ThreadUnlock(cft_vscope);
   return;
   }

if (ptr == VSCOPE)
   {
   VSCOPE = ptr->next;
   }
else
   {
   prev->next = ptr->next;
   }

DeleteHashes(ptr->hashtable);

free(ptr->scope);
free((char *)ptr);

ThreadUnlock(cft_vscope);
}

/*******************************************************************/

void DeleteFromScope(char *scope,struct Rlist *args)

{ struct Rlist *rp;
 char *lval;

for (rp = args; rp != NULL; rp=rp->next)
   {
   lval = (char *)rp->item;
   DeleteScalar(scope,lval);
   }
}

/*******************************************************************/

void CopyScope(char *new, char *old)
/*
 * Thread safe
 */
{ struct Scope *op, *np;
 
Debug("\n*\nCopying scope data %s to %s\n*\n",old,new);

NewScope(new);

if (!ThreadLock(cft_vscope))
   {
   CfOut(cf_error, "", "!! Could not lock VSCOPE");
   return;
   }

if ((op = GetScope(old)))
   {
   np = GetScope(new);
   CopyHashes(np->hashtable,op->hashtable);
   }

ThreadUnlock(cft_vscope);
}

/*******************************************************************/

void ShowScope(char *name)
/*
 * Not thread safe - access to global variable
 */
{ struct Scope *ptr;

for (ptr = VSCOPE; ptr != NULL; ptr=ptr->next)
   {
   if (name && strcmp(ptr->scope,name) != 0)
      {
      continue;
      }

   printf("\nConstant variables in SCOPE %s:\n",ptr->scope);
   PrintHashes(stdout,ptr->hashtable,0);
   }
}

/*******************************************************************/
/* Stack frames                                                    */
/*******************************************************************/

void PushThisScope()

{ struct Scope *op;
 char name[CF_MAXVARSIZE];

op = GetScope("this");

if (op == NULL)
   {
   return;
   }

CF_STCKFRAME++;
PushStack(&CF_STCK,(void *)op);
snprintf(name,CF_MAXVARSIZE,"this_%d",CF_STCKFRAME);
free(op->scope);
op->scope = strdup(name);
}

/*******************************************************************/

void PopThisScope()

{ struct Scope *op = NULL;

if (CF_STCKFRAME > 0)
   {
   DeleteScope("this");
   PopStack(&CF_STCK,(void *)&op,sizeof(op));
   if (op == NULL)
      {
      return;
      }
   
   CF_STCKFRAME--;
   free(op->scope);
   op->scope = strdup("this");
   }    
}


