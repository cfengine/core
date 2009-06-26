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
/* File: hashes.c                                                            */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"


/*******************************************************************/
/* Hashes                                                          */
/*******************************************************************/

void InitHashes(struct CfAssoc **table)

{ int i;

for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   table[i] = NULL;
   }
}

/*******************************************************************/

void BlankHashes(char *scope)

{ int i;
  struct Scope *ptr;

for (ptr = VSCOPE; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->scope,scope) == 0)
      {
      CfOut(cf_verbose,"","Clearing macros in scope(%s)\n",scope);
      
      for (i = 0; i < CF_HASHTABLESIZE; i++)
         {
         if (ptr->hashtable[i] != NULL)
            {
            DeleteAssoc(ptr->hashtable[i]);
            ptr->hashtable[i] = NULL;
            }
         }
      }
   }
}

/******************************************************************/

void CopyHashes(struct CfAssoc **newhash,struct CfAssoc **oldhash)

{ int i;

/* Involved no memory copying, as this is just pointers */

for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   newhash[i] = CopyAssoc(oldhash[i]);
   }
}

/******************************************************************/

void EditHashValue(char *scopeid,char *lval,void *rval)

{ int found, slot, i = slot = GetHash(lval);
  struct Scope *ptr = GetScope(scopeid);
  struct CfAssoc *ap;

Debug("EditHashValue(%s,%s)\n",scopeid,lval);
  
if (CompareVariable(lval,ptr->hashtable[slot]) != 0)
   {
   /* Recover from hash collision */
   
   while (true)
      {
      i++;

      if (i >= CF_HASHTABLESIZE-1)
         {
         i = 0;
         }

      if (CompareVariable(lval,ptr->hashtable[i]) == 0)
         {
         found = true;
         break;
         }

      /* Removed autolookup in Unix environment variables -
         implement as getenv() fn instead */

      if (i == slot)
         {
         found = false;
         break;
         }
      }

   if (!found)
      {
      Debug("No such variable found %s.%s\n",scopeid,lval);
      return;
      }
   }

ap = ptr->hashtable[i];
ap->rval = rval;
}
   
/******************************************************************/

void DeleteHashes(struct CfAssoc **hashtable)

{ int i;

/* Involved no memory copying, as this is just pointers */

for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   for (i = 0; i < CF_HASHTABLESIZE; i++)
      {
      if (hashtable[i] != NULL)
         {
         DeleteAssoc(hashtable[i]);
         hashtable[i] = NULL;
         }
      }
   }
}

/*******************************************************************/

void PrintHashes(FILE *fp,struct CfAssoc **table,int html)

{ int i;

if (html)
   {
   fprintf(fp,"<table class=border width=600>\n");
   fprintf (fp,"<tr><th>id</th><th>dtype</th><th>rtype</th><th>identifier</th><th>Rvalue</th></tr>\n");         
   }
 
for (i = 0; i < CF_HASHTABLESIZE; i++)
   {
   if (table[i] != NULL)
      {
      if (html)
         {
         fprintf (fp,"<tr><td> %5d </td><th>%8s</th><td> %c</td><td> %s</td><td> ",i,CF_DATATYPES[table[i]->dtype],table[i]->rtype,table[i]->lval);
         ShowRval(fp,table[i]->rval,table[i]->rtype);
         fprintf(fp,"</td></tr>\n");         
         }
      else
         {
         fprintf (fp," %5d : %8s %c %s = ",i,CF_DATATYPES[table[i]->dtype],table[i]->rtype,table[i]->lval);
         ShowRval(fp,table[i]->rval,table[i]->rtype);
         fprintf(fp,"\n");
         }
      }
   }

if (html)
   {
   fprintf(fp,"</table>\n");
   }
}

/*******************************************************************/

int GetHash(char *name)

{ int i, slot = 0;

for (i = 0; name[i] != '\0'; i++)
   {
   slot = (CF_MACROALPHABET * slot + name[i]) % CF_HASHTABLESIZE;
   }

return slot;
}

/*******************************************************************/

int AddVariableHash(char *scope,char *lval,void *rval,char rtype,enum cfdatatype dtype,char *fname,int lineno)

{ struct Scope *ptr;
  struct CfAssoc *ap;
  struct Rlist *rp;
  int slot;

if (rtype == CF_SCALAR)
   {
   Debug("AddVariableHash(%s.%s=%s (%s) rtype=%c)\n",scope,lval,rval,CF_DATATYPES[dtype],rtype);
   }
else
   {
   Debug("AddVariableHash(%s.%s=(list) (%s) rtype=%c)\n",scope,lval,CF_DATATYPES[dtype],rtype);
   }

if (lval == NULL || scope == NULL)
   {
   CfOut(cf_error,"","scope.value = %s.%s = %s",scope,lval,rval);
   ReportError("Bad variable or scope in a variable assignment");
   FatalError("Should not happen - forgotten to register a function call in fncall.c?");
   }

if (rval == NULL)
   {
   Debug("No value to assignment - probably a parameter in an unused bundle/body\n");
   return false;
   }

if (strlen(lval) > CF_MAXVARSIZE)
   {
   ReportError("variable lval too long");
   return false;
   }

switch (rtype)
   {
   case CF_SCALAR:

       if (StringContainsVar((char *)rval,lval))
          {
          CfOut(cf_error,"","Scalar variable %s contains itself (non-convergent): %s",lval,(char *)rval);
          return false;
          }
       break;

   case CF_LIST:

       for (rp = rval; rp != NULL; rp=rp->next)
          {
          if (StringContainsVar((char *)rp->item,lval))
             {
             CfOut(cf_error,"","List variable %s contains itself (non-convergent)",lval);
             return false;
             }
          }
       break;
       
   }

ptr = GetScope(scope);
ap = NewAssoc(lval,rval,rtype,dtype);
slot = GetHash(lval);

if (ptr == NULL)
   {
   return false;
   }

while (ptr->hashtable[slot])
   {
   Debug("Hash table Collision! - slot %d = (%s|%s)\n",slot,lval,ptr->hashtable[slot]->lval);

   if (CompareVariable(lval,ptr->hashtable[slot]) == 0)
      {
      if (CompareVariableValue(rval,rtype,ptr->hashtable[slot]) == 0)
         {
         return true;
         }

      if (UnresolvedVariables(ptr->hashtable[slot],rtype) != 0)
         {
         CfOut(cf_inform,"","Duplicate selection of value for variable \"%s\" (broken promise) in scope %s",lval,ptr->scope);
      
         if (fname)
            {
            CfOut(cf_inform,"","Rule from %s at/before line %d\n",fname,lineno);
            }
         else
            {
            CfOut(cf_inform,"","in bundle parameterization\n",fname,lineno);
            }
         }

      DeleteAssoc(ptr->hashtable[slot]);
      ptr->hashtable[slot] = ap;
      Debug("Stored %s in context %s\n",lval,scope);
      return true;
      }
   else
      {
      Debug("Recover from collision\n");

      if (++slot >= CF_HASHTABLESIZE-1)
         {
         slot = 0;
         }
      }
   }

ptr->hashtable[slot] = ap;   
Debug("Added Variable %s at hash address %d in scope %s with value (omitted)\n",lval,slot,scope);
return true;
}


/*******************************************************************/

void DeRefListsInHashtable(char *scope,struct Rlist *namelist,struct Rlist *dereflist)

{ int i, len;
  struct Scope *ptr;
  struct Rlist *rp,*state;
  struct CfAssoc *cphash,*cplist;

if ((len = RlistLen(namelist)) != RlistLen(dereflist))
   {
   FatalError("Software Error DeRefLists... correlated lists not same length");
   }

if (len == 0)
   {
   return;
   }

for (ptr = VSCOPE; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->scope,scope) == 0)
      {
      for (i = 0; i < CF_HASHTABLESIZE; i++)
         {
         cphash = ptr->hashtable[i];
         
         if (cphash != NULL)
            {
            for (rp = dereflist; rp != NULL; rp = rp->next)
               {
               cplist = (struct CfAssoc *)rp->item;

               if (strcmp(cplist->lval,cphash->lval) == 0)
                  {
                  /* Link up temp hash to variable lol */

// state = rp->state_ptr;
                  state = (struct Rlist *)(cplist->rval);

                  if (rp->state_ptr)
                     {
                     Debug("Rewriting expanded type for %s from %s to %s\n",cphash->lval,CF_DATATYPES[cphash->dtype],rp->state_ptr->item);
                  
                     cphash->rval = rp->state_ptr->item;
                     }
                                    
                  switch(cphash->dtype)
                     {
                     case cf_slist:
                         cphash->dtype = cf_str;
                         cphash->rtype = CF_SCALAR;
                         break;
                     case cf_ilist:
                         cphash->dtype = cf_int;
                         cphash->rtype = CF_SCALAR;
                         break;
                     case cf_rlist:
                         cphash->dtype = cf_real;
                         cphash->rtype = CF_SCALAR;
                         break;
                     }

                  Debug(" to %s\n",CF_DATATYPES[cphash->dtype]);
                  }
               }
            }
         }
      }
   }
}
