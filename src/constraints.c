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
/* File: constraints.c                                                       */
/*                                                                           */
/* Created: Wed Oct 17 13:00:08 2007                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*******************************************************************/

struct Constraint *AppendConstraint(struct Constraint **conlist,char *lval, void *rval, char type,char *classes,int body)

/* Note rval must be pre-allocated for this function, e.g. use
   CopyRvalItem in call.  This is to make the parser and var expansion
   non-leaky */
    
{ struct Constraint *cp,*lp;
  char *sp = NULL;

switch(type)
   {
   case CF_SCALAR:
       Debug("   Appending Constraint: %s => %s\n",lval,rval);
       
       if (PARSING && strcmp(lval,"ifvarclass") == 0)
          {
          ValidateClassSyntax(rval);
          }

       break;
   case CF_FNCALL:
       Debug("   Appending a function call to rhs\n");
       break;
   case CF_LIST:
       Debug("   Appending a list to rhs\n");
   }

if ((cp = (struct Constraint *)malloc(sizeof(struct Constraint))) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate Constraint");
   FatalError("");
   }

if ((sp = strdup(lval)) == NULL)
   {
   CfOut(cf_error,"strdup","Unable to allocate Constraint lval");
   FatalError("");
   }

if (*conlist == NULL)
   {
   *conlist = cp;
   }
else
   {
   for (lp = *conlist; lp->next != NULL; lp=lp->next)
      {
      }

   lp->next = cp;
   }

if (classes != NULL)
   {
   if ((cp->classes = strdup(classes)) == NULL)
      {
      CfOut(cf_error,"mstrdup","Unable to allocate Constraint classes");
      FatalError("");
      }
   }
else
   {
   cp->classes = NULL;
   }

cp->audit = AUDITPTR;
cp->lineno = P.line_no;
cp->lval = sp;
cp->rval = rval;
cp->type = type;  /* literal, bodyname, builtin function */
cp->isbody = body;
cp->next = NULL;
return cp;
}

/*****************************************************************************/

void EditScalarConstraint(struct Constraint *conlist,char *lval,char *rval)

{ struct Constraint *cp;
 
for (cp = conlist; cp != NULL; cp = cp->next)
   {
   if (strcmp(lval,cp->lval) == 0)
      {
      if (cp->rval)
         {
         DeleteRvalItem(cp->rval,cp->type);
         }      

      cp->rval = strdup(rval);
      cp->type = CF_SCALAR;
      return;
      }
   }
}

/*****************************************************************************/

void DeleteConstraintList(struct Constraint *conlist)

{ struct Constraint *cp, *next;

Debug("DeleteConstraintList()\n");
 
for (cp = conlist; cp != NULL; cp = next)
   {
   Debug("Delete lval = %s,%c\n",cp->lval,cp->type);
   
   next = cp->next;

   if (cp->rval)
      {
      DeleteRvalItem(cp->rval,cp->type);
      }

   if (cp->lval)
      {
      free((char *)cp->lval);
      }
   
   if (cp->classes)
      {
      free(cp->classes);
      }

   free((char *)cp);
   }
}

/*****************************************************************************/

int GetBooleanConstraint(char *lval,struct Promise *pp)

{ struct Constraint *cp;
  int retval = CF_UNDEFINED;

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,lval) == 0)
      {
      if (IsDefinedClass(cp->classes))
         {
         if (retval != CF_UNDEFINED)
            {
            CfOut(cf_error,""," !! Multiple \"%s\" (boolean) constraints break this promise\n",lval);
            PromiseRef(cf_error,pp);
            }
         }
      else
         {
         continue;
         }

      if (cp->type != CF_SCALAR)
         {
         CfOut(cf_error,"","Anomalous type mismatch - expected type (%c) for boolean constraint %s did not match internals\n",cp->type,lval);
         PromiseRef(cf_error,pp);
         FatalError("Aborted");
         }

      if (strcmp(cp->rval,"true") == 0||strcmp(cp->rval,"yes") == 0)
         {
         retval = true;
         continue;
         }

      if (strcmp(cp->rval,"false") == 0||strcmp(cp->rval,"no") == 0)
         {
         retval = false;
         }
      }
   }

if (retval == CF_UNDEFINED)
   {
   retval = false;
   }

return retval;
}

/*****************************************************************************/

int GetRawBooleanConstraint(char *lval,struct Constraint *list)

{ struct Constraint *cp;
  int retval = CF_UNDEFINED;

for (cp = list; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,lval) == 0)
      {
      if (IsDefinedClass(cp->classes))
         {
         if (retval != CF_UNDEFINED)
            {
            CfOut(cf_error,""," !! Multiple \"%s\" (boolean) body constraints break this promise\n",lval);
            }
         }
      else
         {
         continue;
         }

      if (cp->type != CF_SCALAR)
         {
         CfOut(cf_error,"","Anomalous type mismatch - expected type (%c) for boolean constraint %s did not match internals\n",cp->type,lval);
         FatalError("Aborted");
         }

      if (strcmp(cp->rval,"true") == 0||strcmp(cp->rval,"yes") == 0)
         {
         retval = true;
         continue;
         }

      if (strcmp(cp->rval,"false") == 0||strcmp(cp->rval,"no") == 0)
         {
         retval = false;
         }
      }
   }

if (retval == CF_UNDEFINED)
   {
   retval = false;
   }

return retval;
}

/*****************************************************************************/

int GetBundleConstraint(char *lval,struct Promise *pp)

{ struct Constraint *cp;
  int retval = CF_UNDEFINED;

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,lval) == 0)
      {
      if (IsDefinedClass(cp->classes))
         {
         if (retval != CF_UNDEFINED)
            {
            CfOut(cf_error,""," !! Multiple \"%s\" constraints break this promise\n",lval);
            PromiseRef(cf_error,pp);
            }
         }
      else
         {
         continue;
         }

      if (!(cp->type == CF_FNCALL || cp->type == CF_SCALAR))
         {
         CfOut(cf_error,"","Anomalous type mismatch - type (%c) for bundle constraint %s did not match internals\n",cp->type,lval);
         PromiseRef(cf_error,pp);
         FatalError("Aborted");
         }

      return true;
      }
   }

return false;
}

/*****************************************************************************/

int GetIntConstraint(char *lval,struct Promise *pp)

{ struct Constraint *cp;
  int retval = CF_NOINT;

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,lval) == 0)
      {
      if (IsDefinedClass(cp->classes))
         {
         if (retval != CF_NOINT)
            {
            CfOut(cf_error,""," !! Multiple \"%s\" (int) constraints break this promise\n",lval);
            PromiseRef(cf_error,pp);
            }
         }
      else
         {
         continue;
         }

      if (cp->type != CF_SCALAR)
         {
         CfOut(cf_error,"","Anomalous type mismatch - expected type for int constraint %s did not match internals\n",lval);
         PromiseRef(cf_error,pp);
         FatalError("Aborted");
         }

      retval = (int)Str2Int((char *)cp->rval);
      }
   }

return retval;
}

/*****************************************************************************/

double GetRealConstraint(char *lval,struct Promise *pp)

{ struct Constraint *cp;
  double retval = CF_NODOUBLE;

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,lval) == 0)
      {
      if (IsDefinedClass(cp->classes))
         {
         if (retval != CF_NODOUBLE)
            {
            CfOut(cf_error,""," !! Multiple \"%s\" (real) constraints break this promise\n",lval);
            }
         }
      else
         {
         continue;
         }

      if (cp->type != CF_SCALAR)
         {
         CfOut(cf_error,"","Anomalous type mismatch - expected type for int constraint %s did not match internals\n",lval);
         FatalError("Aborted");
         }

      retval = Str2Double((char *)cp->rval);
      }
   }

return retval;
}

/*****************************************************************************/

mode_t GetOctalConstraint(char *lval,struct Promise *pp)

{ struct Constraint *cp;
  mode_t retval = 077;

// We could handle units here, like kb,b,mb
  
for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,lval) == 0)
      {
      if (IsDefinedClass(cp->classes))
         {
         if (retval != 077)
            {
            CfOut(cf_error,""," !! Multiple \"%s\" (int,octal) constraints break this promise\n",lval);
            PromiseRef(cf_error,pp);
            }
         }
      else
         {
         continue;
         }

      if (cp->type != CF_SCALAR)
         {
         CfOut(cf_error,"","Anomalous type mismatch - expected type for int constraint %s did not match internals\n",lval);
         PromiseRef(cf_error,pp);
         FatalError("Aborted");
         }

      retval = Str2Mode((char *)cp->rval);
      }
   }

return retval;
}

/*****************************************************************************/

uid_t GetUidConstraint(char *lval,struct Promise *pp)

#ifdef MINGW
{  // we use sids on windows instead
  return CF_SAME_OWNER;
}
#else  /* NOT MINGW */
{ struct Constraint *cp;
  int retval = CF_SAME_OWNER;
  char buffer[CF_MAXVARSIZE];

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,lval) == 0)
      {
      if (IsDefinedClass(cp->classes))
         {
         if (retval != CF_UNDEFINED)
            {
            CfOut(cf_error,""," !! Multiple \"%s\" (owner/uid) constraints break this promise\n",lval);
            PromiseRef(cf_error,pp);
            }
         }
      else
         {
         continue;
         }

      if (cp->type != CF_SCALAR)
         {
         CfOut(cf_error,"","Anomalous type mismatch - expected type for owner constraint %s did not match internals\n",lval);
         PromiseRef(cf_error,pp);
         FatalError("Aborted");
         }

      retval = Str2Uid((char *)cp->rval,buffer,pp);
      }
   }

return retval;
}
#endif  /* NOT MINGW */

/*****************************************************************************/

gid_t GetGidConstraint(char *lval,struct Promise *pp)

#ifdef MINGW
{  // not applicable on windows: processes have no group
  return CF_SAME_GROUP;
}
#else
{ struct Constraint *cp;
  int retval = CF_SAME_OWNER;
  char buffer[CF_MAXVARSIZE];
    
for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,lval) == 0)
      {
      if (IsDefinedClass(cp->classes))
         {
         if (retval != CF_UNDEFINED)
            {
            CfOut(cf_error,""," !! Multiple \"%s\"  (group/gid) constraints break this promise\n",lval);
            PromiseRef(cf_error,pp);
            }
         }
      else
         {
         continue;
         }

      if (cp->type != CF_SCALAR)
         {
         CfOut(cf_error,"","Anomalous type mismatch - expected type for group constraint %s did not match internals\n",lval);
         PromiseRef(cf_error,pp);
         FatalError("Aborted");
         }

      retval = Str2Gid((char *)cp->rval,buffer,pp);
      }
   }

return retval;
}
#endif  /* NOT MINGW */

/*****************************************************************************/

struct Rlist *GetListConstraint(char *lval,struct Promise *pp)

{ struct Constraint *cp;
  struct Rlist *retval = NULL;

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,lval) == 0)
      {
      if (IsDefinedClass(cp->classes))
         {
         if (retval != NULL)
            {
            CfOut(cf_error,""," !! Multiple \"%s\" int constraints break this promise\n",lval);
            PromiseRef(cf_error,pp);
            }
         }
      else
         {
         continue;
         }

      if (cp->type != CF_LIST)
         {
         CfOut(cf_error,"","Anomalous type mismatch - expected type for list constraint %s did not match internals\n",lval);
         PromiseRef(cf_error,pp);
         FatalError("Aborted");
         }

      retval = (struct Rlist *)cp->rval;
      }
   }

return retval;
}

/*****************************************************************************/

void *GetConstraint(char *lval,struct Promise *pp,char rtype)

{ struct Constraint *cp;
  void *retval = NULL;

if (pp == NULL)
   {
   return NULL;
   }
  
if (!VerifyConstraintName(lval))
   {
   CfOut(cf_error,""," !! Self-diagnostic: Constraint type \"%s\" is not a registered type\n",lval);
   }

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,lval) == 0)
      {
      if (IsDefinedClass(cp->classes))
         {
         if (retval != NULL)
            {
            CfOut(cf_error,""," !! Inconsistent \"%s\" constraints break this promise\n",lval);
            PromiseRef(cf_error,pp);
            }

         retval = cp->rval;

         if (cp->type != rtype)
            {
            return NULL;
            }
         }
      }
   }

return retval;
}

/*****************************************************************************/

void ReCheckAllConstraints(struct Promise *pp)

{ struct Constraint *cp;
 char *sp,*handle = GetConstraint("handle",pp,CF_SCALAR);
  struct PromiseIdent *prid;
  struct Item *ptr;
  int in_class_any = false;

if (strcmp(pp->agentsubtype,"reports") == 0 && strcmp(pp->classes,"any") == 0)
   {
   char *cl = GetConstraint("ifvarclass",pp,CF_SCALAR);
   
   if (cl == NULL || strcmp(cl,"any") == 0)
      {
      in_class_any = true;
      }
   
   if (in_class_any)
      {
      CfOut(cf_error,"","reports promises may not be in class \'any\' - risk of a notification explosion");
      PromiseRef(cf_error,pp);
      }
   }


/* Special promise type checks */

if (!IsDefinedClass(pp->classes))
   {
   return;
   }

if (VarClassExcluded(pp,&sp))
   {
   return;
   }

if (SHOWREPORTS)
   {
   NewPromiser(pp);
   }

if (handle)
   {
   if (!ThreadLock(cft_policy))
      {
      CfOut(cf_error, "", "!! Could not lock cft_policy in ReCheckAllConstraints() -- aborting");
      return;
      }
   
   if ((prid = PromiseIdExists(handle)))
      {
      if ((strcmp(prid->filename,pp->audit->filename) != 0) || (prid->lineno != pp->lineno))
         {
         CfOut(cf_error,""," !! Duplicate promise handle -- previously used in file %s near line %d",prid->filename,prid->lineno);
         PromiseRef(cf_error,pp);
         }
      }
   else
      {
      NewPromiseId(handle,pp);
      }
   
   prid = NULL; // we can't access this after unlocking
   ThreadUnlock(cft_policy);
   }

if (REQUIRE_COMMENTS == true)
   {
   if (pp->ref == NULL && strcmp(pp->agentsubtype,"vars") != 0)
      {
      CfOut(cf_error,""," !! Un-commented promise found, but comments have been required by policy\n");
      PromiseRef(cf_error,pp);
      }
   }

for (cp = pp->conlist; cp != NULL; cp = cp->next)
   {
   PostCheckConstraint(pp->agentsubtype,pp->bundle,cp->lval,cp->rval,cp->type);
   }     

if (strcmp(pp->agentsubtype,"insert_lines") == 0)
   {
   /* Multiple additions with same criterion will not be convergent -- but ignore for empty file baseline */
   
   if ((sp = GetConstraint("select_line_matching",pp,CF_SCALAR)))
      {
      if ((ptr = ReturnItemIn(EDIT_ANCHORS,sp)))
         {
         if (strcmp(ptr->classes,pp->bundle) == 0)
            {
            CfOut(cf_inform,""," !! insert_lines promise uses the same select_line_matching anchor (\"%s\") as another promise. This will lead to non-convergent behaviour unless \"empty_file_before_editing\" is set.",sp);
            PromiseRef(cf_inform,pp);
            }
         }
      else
         {
         PrependItem(&EDIT_ANCHORS,sp,pp->bundle);
         }
      }
   }

PreSanitizePromise(pp);
}

/*****************************************************************************/

void PostCheckConstraint(char *type,char *bundle,char *lval,void *rval,char rvaltype)

{ struct SubTypeSyntax ss;
  int i,j,l,m;
  struct BodySyntax *bs,*bs2;
  struct SubTypeSyntax *ssp;

Debug("  Post Check Constraint %s: %s =>",type,lval);

if (DEBUG)
   {
   ShowRval(stdout,rval,rvaltype);
   printf("\n");
   }

for  (i = 0; i < CF3_MODULES; i++)
   {
   if ((ssp = CF_ALL_SUBTYPES[i]) == NULL)
      {
      continue;
      }

   for (j = 0; ssp[j].btype != NULL; j++)
      {
      ss = ssp[j];

      if (ss.subtype != NULL) 
         {
         if (strcmp(ss.subtype,type) == 0)
            {
            bs = ss.bs;
            
            for (l = 0; bs[l].lval != NULL; l++)
               {
               if (bs[l].dtype == cf_bundle)
                  {
                  }
               else if (bs[l].dtype == cf_body)
                  {
                  bs2 = (struct BodySyntax *)bs[l].range;
                  
                  for (m = 0; bs2[m].lval != NULL; m++)
                     {
                     if (strcmp(lval,bs2[m].lval) == 0)
                        {
                        CheckConstraintTypeMatch(lval,rval,rvaltype,bs2[m].dtype,(char *)(bs2[m].range),0);
                        return;
                        }
                     }                  
                  }

               if (strcmp(lval,bs[l].lval) == 0)
                  {
                  CheckConstraintTypeMatch(lval,rval,rvaltype,bs[l].dtype,(char *)(bs[l].range),0);
                  return;
                  }
               }                        
            }
         }
      }
   }

/* Now check the functional modules - extra level of indirection */

for (i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
   {
   if (CF_COMMON_BODIES[i].dtype == cf_body)
      {
      continue;
      }
   
   if (strcmp(lval,CF_COMMON_BODIES[i].lval) == 0)
      {
      Debug("Found a match for lval %s in the common constraint attributes\n",lval);
      CheckConstraintTypeMatch(lval,rval,rvaltype,CF_COMMON_BODIES[i].dtype,(char *)(CF_COMMON_BODIES[i].range),0);
      return;
      }
   }
}

/*****************************************************************************/

int VerifyConstraintName(char *lval)

{ struct SubTypeSyntax ss;
  int i,j,l,m;
  struct BodySyntax *bs,*bs2;
  struct SubTypeSyntax *ssp;

Debug("  Verify Constrant name %s\n",lval);

for  (i = 0; i < CF3_MODULES; i++)
   {
   if ((ssp = CF_ALL_SUBTYPES[i]) == NULL)
      {
      continue;
      }

   for (j = 0; ssp[j].btype != NULL; j++)
      {
      ss = ssp[j];

      if (ss.subtype != NULL) 
         {
         bs = ss.bs;
         
         for (l = 0; bs[l].lval != NULL; l++)
            {
            if (bs[l].dtype == cf_bundle)
               {
               }
            else if (bs[l].dtype == cf_body)
               {
               bs2 = (struct BodySyntax *)bs[l].range;
               
               for (m = 0; bs2[m].lval != NULL; m++)
                  {
                  if (strcmp(lval,bs2[m].lval) == 0)
                     {
                     return true;
                     }
                  }                  
               }
            
            if (strcmp(lval,bs[l].lval) == 0)
               {
               return true;
               }
            }                        
         }
      }
   }

/* Now check the functional modules - extra level of indirection */

for (i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
   {
   if (strcmp(lval,CF_COMMON_BODIES[i].lval) == 0)
      {
      return true;
      }
   }

return false;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

// NOTE: PROMISE_ID_LIST must be thread-safe here (locked by caller)

struct PromiseIdent *NewPromiseId(char *handle,struct Promise *pp)

{ struct PromiseIdent *ptr;

AssertThreadLocked(cft_policy,"NewPromiseId");

if ((ptr = malloc(sizeof(struct PromiseIdent))) == NULL)
   {
   FatalError("MemoryAlloc NewPromiseId\n");
   }

ptr->filename = strdup(pp->audit->filename);
ptr->lineno = pp->lineno;
ptr->handle = strdup(handle);
ptr->next = PROMISE_ID_LIST;
PROMISE_ID_LIST = ptr;
return ptr;     
}

/*****************************************************************************/

void DeleteAllPromiseIdsRecurse(struct PromiseIdent *key)

{
AssertThreadLocked(cft_policy, "DeleteAllPromiseIdsRecurse");

if (key->next != NULL)
   {
   DeleteAllPromiseIdsRecurse(key->next);
   }

free(key->filename);
free(key->handle);
free(key);
}

/*****************************************************************************/

void DeleteAllPromiseIds()

{
if (!ThreadLock(cft_policy))
   {
   CfOut(cf_error, "", "!! Could not lock cft_policy in DelteAllPromiseIds() -- aborting");
   return;
   }

if (PROMISE_ID_LIST)
   {
   DeleteAllPromiseIdsRecurse(PROMISE_ID_LIST);
   PROMISE_ID_LIST = NULL;
   }

ThreadUnlock(cft_policy);
}

/*****************************************************************************/

struct PromiseIdent *PromiseIdExists(char *handle)

{ struct PromiseIdent *key;

AssertThreadLocked(cft_policy, "PromiseIdExists");

for (key = PROMISE_ID_LIST; key != NULL; key=key->next)
   {
   if (strcmp(handle,key->handle) == 0)
      {
      return key;
      }
   }

return NULL;
}

