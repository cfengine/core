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
/* File: iteration.c                                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

struct Rlist *NewIterationContext(char *scopeid,struct Rlist *namelist)

{ struct Rlist *this,*rp,*rps,*deref_listoflists = NULL;
  char rtype;
  void *returnval;
  enum cfdatatype dtype;
  struct Scope *ptr = NULL;
  struct CfAssoc *new;
  struct Rval newret;

Debug("\n*\nNewIterationContext(from %s)\n*\n",scopeid);

CopyScope("this",scopeid);

ptr=GetScope("this");

if (namelist == NULL)
   {
   Debug("No lists to iterate over\n");
   return NULL;
   }

for (rp = namelist; rp != NULL; rp = rp->next)
   {
   dtype = GetVariable(scopeid,rp->item,&returnval,&rtype);
   
   if (dtype == cf_notype)
      {
      CfOut(cf_error,""," !! Couldn't locate variable %s apparently in %s\n",rp->item,scopeid);
      CfOut(cf_error,""," !! Could be incorrect use of a global iterator -- see reference manual on list substitution");
      continue;
      }
   
   /* Make a copy of list references in scope only, without the names */

   if (rtype == CF_LIST)
      {
      for (rps = (struct Rlist *)returnval; rps != NULL; rps=rps->next)
         {
         if (rps->type == CF_FNCALL)
            {
            struct FnCall *fp = (struct FnCall *)rps->item;
            newret = EvaluateFunctionCall(fp,NULL);
            DeleteFnCall(fp);
            rps->item = newret.item;
            rps->type = newret.rtype;
            }
         }
      }

   if ((new = NewAssoc(rp->item,returnval,rtype,dtype)))
      {
      this = OrthogAppendRlist(&deref_listoflists,new,CF_LIST);
      rp->state_ptr = new->rval;
      
      while (rp->state_ptr && strcmp(rp->state_ptr->item,CF_NULL_VALUE) == 0)
         {
         if (rp->state_ptr)
            {
            rp->state_ptr = rp->state_ptr->next;
            }
         }
      }
   }

/* We now have a control list of list-variables, with internal state in state_ptr */

return deref_listoflists;
}

/*****************************************************************************/

void DeleteIterationContext(struct Rlist *deref)

{
DeleteScope("this");

if (deref != NULL)
   {
   DeleteReferenceRlist(deref);
   }
}

/*****************************************************************************/

int IncrementIterationContext(struct Rlist *iterator,int level)

{ struct Rlist *state;
  struct CfAssoc *cp;
  
if (iterator == NULL)
   {
   return false;
   }

// iterator->next points to the next list
// iterator->state_ptr points to the current item in the current list

cp = (struct CfAssoc *)iterator->item;
state = iterator->state_ptr;

if (state == NULL)
   {
   return false;
   }

/* Go ahead and increment */

Debug(" -> Incrementing (%s) from \"%s\"\n",cp->lval,iterator->state_ptr->item);

if (state->next == NULL)
   {
   /* This wheel has come to full revolution, so move to next */
   
   if (iterator->next != NULL)
      {
      /* Increment next wheel */

      if (IncrementIterationContext(iterator->next,level+1))
         {
         /* Not at end yet, so reset this wheel */
         iterator->state_ptr = cp->rval;
         iterator->state_ptr = iterator->state_ptr->next;         
         return true;
         }
      else
         {
         /* Reached last variable wheel - pass up */
         return false;
         }
      }
   else
      {
      /* Reached last variable wheel - waiting for end detection */
      return false;
      }
   }
else
   {
   /* Update the current wheel */
   iterator->state_ptr = state->next;

   Debug(" <- Incrementing wheel (%s) to \"%s\"\n",cp->lval,iterator->state_ptr->item);

   while (iterator->state_ptr && strcmp(iterator->state_ptr->item,CF_NULL_VALUE) == 0)
      {
      if (IncrementIterationContext(iterator->next,level+1))
         {
         /* Not at end yet, so reset this wheel (next because we always start with cf_null now) */
         iterator->state_ptr = cp->rval;
         iterator->state_ptr = iterator->state_ptr->next;         
         return true;
         }
      else
         {
         /* Reached last variable wheel - pass up */
         break;
         }
      }

   if (EndOfIteration(iterator))
      {
      return false;
      }

   return true;
   }
}

/*****************************************************************************/

int EndOfIteration(struct Rlist *iterator)

{ struct Rlist *rp,*state;

if (iterator == NULL)
   {
   return true;
   }

/* When all the wheels are at NULL, we have reached the end*/

for (rp = iterator; rp != NULL; rp = rp->next)
   {
   state = rp->state_ptr;

   if (state == NULL)
      {
      continue;
      }

   if (state && state->next != NULL)
      {
      return false;
      }
   }

return true;
}

/*****************************************************************************/

int NullIterators(struct Rlist *iterator)

{ struct Rlist *rp,*state;

if (iterator == NULL)
   {
   return false;
   }

/* When all the wheels are at NULL, we have reached the end*/

for (rp = iterator; rp != NULL; rp = rp->next)
   {
   state = rp->state_ptr;

   if (state && strcmp(state->item,CF_NULL_VALUE) == 0)
      {
      return true;
      }
   }

return false;
}

/*******************************************************************/

void DeleteReferenceRlist(struct Rlist *list)

/* Delete all contents, hash table in scope has own copy */
{
if (list == NULL)
   {
   return;
   }

DeleteAssoc((struct CfAssoc *)list->item);


DeleteReferenceRlist(list->next);
free((char *)list);
}
