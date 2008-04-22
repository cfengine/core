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
/* File: scope.c                                                             */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"


/*******************************************************************/

struct Scope *GetScope(char *scope)

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

void SetNewScope(char *id)

{
NewScope(id);
strncpy(CONTEXTID,id,31);
}

/*******************************************************************/

void NewScope(char *name)

{ struct Scope *ptr;
  
Debug1("Adding scope data %s\n", name);

for (ptr = VSCOPE; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->scope,name) == 0)
      {
      Debug("Object %s already exists\n",name);
      return;
      }
   }

if ((ptr = (struct Scope *)malloc(sizeof(struct Scope))) == NULL)
   {
   FatalError("Memory Allocation failed for Scope");
   }

InitHashes((struct CfAssoc**)ptr->hashtable);

ptr->next = VSCOPE;
ptr->scope = strdup(name);
VSCOPE = ptr; 
}

/*******************************************************************/

void DeleteAllScope()

{ struct Scope *ptr, *this;
  
Debug1("Deleting all scoped variables\n");

ptr = VSCOPE;

while (ptr != NULL)
   {
   this = ptr;
   DeleteHashes(this->hashtable);
   free(this->scope);   
   ptr = this->next;
   free((char *)this);
   }

VSCOPE = NULL;
}

/*******************************************************************/

void DeleteScope(char *name)

{ struct Scope *ptr, *prev = NULL;
  
Debug1("Deleting scope %s\n", name);

for (ptr = VSCOPE; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->scope,name) == 0)
      {
      Debug("Object %s exists\n",name);
      break;
      }
   
   prev = ptr;
   }

if (ptr == NULL)
   {
   Debug("No such scope to delete\n");
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
}

/*******************************************************************/

void CopyScope(char *new, char *old)

{ struct Scope *op, *np;
 
Debug("\n*\nCopying scope data %s to %s\n*\n",old,new);

NewScope(new);
op = GetScope(old);
np = GetScope(new);
CopyHashes(np->hashtable,op->hashtable);
}

