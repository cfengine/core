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
/* File: iteration.c                                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

struct Rlist *NewIterationContext(char *scopeid,struct Rlist *namelist)

{ struct Rlist *this,*rp,*state,*deref_listoflists = NULL;
  char *lval,rtype;
  void *returnval;
  enum cfdatatype dtype;
  struct Scope *ptr = NULL;
  struct CfAssoc *new,*cp;

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
      CfOut(cf_error,"","Software error: Couldn't locate variable %s....in %s\n",rp->item,scopeid);
      FatalError("Failure in scanning promise variables");
      }
   
   /* Make a copy of list references in scope only, without the names */

   new = NewAssoc(rp->item,returnval,rtype,dtype);

   this = OrthogAppendRlist(&deref_listoflists,new,CF_LIST);

   /* Now fix state_ptr so it points to current (rlist) entry */
   
   cp = (struct CfAssoc *)this->item;
   state = (struct Rlist *)cp->rval;
   state->state_ptr = state;
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

cp = (struct CfAssoc *)iterator->item;
state = (struct Rlist *)cp->rval;

if (state->state_ptr == NULL)
   {
   /* If this wheel comes to full revolution ..*/
   
   if (iterator->next != NULL)
      {
      /* Increment next wheel */

      if (IncrementIterationContext(iterator->next,level+1))
         {
         /* Not at end yet, so reset this wheel */
         state->state_ptr = state;
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
      /* Reached last variable wheel */
      return false;
      }
   }
else
   {
   state->state_ptr = (state->state_ptr)->next;
   return true;
   }
}

/*****************************************************************************/

int EndOfIteration(struct Rlist *iterator)

{ struct Rlist *rp,*state_ptr,*state;
  struct CfAssoc *cp;

if (iterator == NULL)
   {
   return true;
   }

for (rp = iterator; rp != NULL; rp = rp->next)
   {
   cp = (struct CfAssoc *)iterator->item;
   state = (struct Rlist *)cp->rval;
   
   if (state->state_ptr != NULL)
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
