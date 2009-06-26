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

{ struct Rlist *this,*rp,*deref_listoflists = NULL;
  char *lval,rtype;
  void *returnval;
  enum cfdatatype dtype;
  struct Scope *ptr = NULL;
  struct CfAssoc *new;

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

   if (new = NewAssoc(rp->item,returnval,rtype,dtype))
      {
      this = OrthogAppendRlist(&deref_listoflists,new,CF_LIST);
      rp->state_ptr = new->rval;
      }
   else
      {
      FatalError("Iteration context failed");
      }
   }

/* We now have a control list of list-variables, with internal state in state_ptr */

return deref_listoflists;
}

/*****************************************************************************/

void DeleteIterationContext(struct Rlist *deref)

{
DeleteScope("this");

/* Cannot use DeleteRlist(deref) as we are referencing memory from hashtable */

if (deref != NULL)
   {
   DeleteReferenceRlist(deref);
   }
}

/*****************************************************************************/

int IncrementIterationContext(struct Rlist *iterator,int level)

{ struct Rlist *rp,*state_ptr,*state;
  struct CfAssoc *cp;

if (iterator == NULL)
   {
   return false;
   }

// iterator->next points to the next list
// iterator->state_ptr points to the current item in the current list

cp = (struct CfAssoc *)iterator->item;
state = iterator->state_ptr;

Debug("Incrementing %s\n",cp->lval);

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
   Debug("Incrementing wheel %s\n",cp->lval);
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
   
   if (state->next != NULL)
      {
      return false;
      }
   }

return true;
}

/*******************************************************************/

void DeleteReferenceRlist(struct Rlist *list)

{
if (list == NULL)
   {
   return;
   }

DeleteAssoc(list->item);

/* Delete infrastructure assuming content remains allocated */

DeleteReferenceRlist(list->next);
free((char *)list);
}
