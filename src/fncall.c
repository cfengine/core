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
/* File: fncall.c                                                            */
/*                                                                           */
/* Created: Wed Aug  8 14:45:53 2007                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

static void PrintFunctions(void);
static void ClearFnCallStatus(void);

/*******************************************************************/

int IsBuiltinFnCall(void *rval,char rtype)

{ int i;
  struct FnCall *fp;

if (rtype != CF_FNCALL)
   {
   return false;
   }

fp = (struct FnCall *)rval;

if (FindFunction(fp->name))
   {
   Debug("%s is a builtin function\n",fp->name);
   return true;
   }
else
   {
   return false;
   }
}

/*******************************************************************/

struct FnCall *NewFnCall(char *name, struct Rlist *args)

{ struct FnCall *fp;
  char *sp = NULL;

Debug("Installing Function Call %s\n",name);

if ((fp = (struct FnCall *)malloc(sizeof(struct FnCall))) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate FnCall");
   FatalError("");
   }

if ((sp = strdup(name)) == NULL)
   {
   CfOut(cf_error,"malloc","Unable to allocate Promise");
   FatalError("");
   }

fp->name = sp;
fp->args = args;
fp->argc = RlistLen(args);

Debug("Installed ");
if (DEBUG)
   {
   ShowFnCall(stdout,fp);
   }
Debug("\n\n");
return fp;
}

/*******************************************************************/

struct FnCall *CopyFnCall(struct FnCall *f)

{
Debug("CopyFnCall()\n");
return NewFnCall(f->name,CopyRlist(f->args));
}

/*******************************************************************/

void DeleteFnCall(struct FnCall *fp)

{
if (fp->name)
   {
   free(fp->name);
   }

if (fp->args)
   {
   DeleteRlist(fp->args);
   }

free(fp);
}

/*********************************************************************/

struct FnCall *ExpandFnCall(char *contextid,struct FnCall *f,int expandnaked)

{
 Debug("ExpandFnCall()\n");
//return NewFnCall(f->name,ExpandList(contextid,f->args,expandnaked));
return NewFnCall(f->name,ExpandList(contextid,f->args,false));
}

/*******************************************************************/

static void PrintFunctions(void)

{
int i;

for (i = 0; i < 3; i++)
   {
   if (P.currentfncall[i] != NULL)
      {
      printf("(%d) =========================\n|",i);
      ShowFnCall(stdout,P.currentfncall[i]);
      printf("|\n==============================\n");
      }
   } 
}

/*******************************************************************/

int PrintFnCall(char *buffer, int bufsize,struct FnCall *fp)
    
{ struct Rlist *rp;
  char work[CF_MAXVARSIZE];

snprintf(buffer,bufsize,"%s(",fp->name);

for (rp = fp->args; rp != NULL; rp=rp->next)
   {
   switch (rp->type)
      {
      case CF_SCALAR:
          Join(buffer,(char *)rp->item,bufsize);
          break;

      case CF_FNCALL:
          PrintFnCall(work,CF_MAXVARSIZE,(struct FnCall *)rp->item);
          Join(buffer,work,bufsize);
          break;

      default:
          break;
      }
   
   if (rp->next != NULL)
      {
      strcat(buffer,",");
      }
   }

 strcat(buffer, ")");

return strlen(buffer);
}

/*******************************************************************/

void ShowFnCall(FILE *fout,struct FnCall *fp)

{ struct Rlist *rp;

if (XML)
   {
   fprintf(fout,"%s(",fp->name);
   }
else
   {
   fprintf(fout,"%s(",fp->name);
   }

for (rp = fp->args; rp != NULL; rp=rp->next)
   {
   switch (rp->type)
      {
      case CF_SCALAR:
          fprintf(fout,"%s,",(char *)rp->item);
          break;

      case CF_FNCALL:
          ShowFnCall(fout,(struct FnCall *)rp->item);
          break;

      default:
          fprintf(fout,"(** Unknown argument **)\n");
          break;
      }
   }

if (XML)
   {
   fprintf(fout,")");
   }
else
   {
   fprintf(fout,")");
   }
}

/*******************************************************************/

enum cfdatatype FunctionReturnType(const char *name)
{
FnCallType *fn = FindFunction(name);
return fn ? fn->dtype : cf_notype;
}

/*******************************************************************/

struct Rval EvaluateFunctionCall(struct FnCall *fp,struct Promise *pp)

{ struct Rlist *expargs;
  struct Rval rval;
  FnCallType *this = FindFunction(fp->name);

rval.item = NULL;
rval.rtype = CF_NOPROMISEE;

if (this)
   {
   if (DEBUG)
      {
      printf("EVALUATE FN CALL %s\n",fp->name);
      ShowFnCall(stdout,fp);
      printf("\n");
      }
   }
else
   {
   if (pp)
      {
      CfOut(cf_error,"","No such FnCall \"%s()\" in promise @ %s near line %d\n",fp->name,pp->audit->filename,pp->lineno);
      }
   else
      {
      CfOut(cf_error,"","No such FnCall \"%s()\" - context info unavailable\n",fp->name);
      }
   
   return rval;
   }


/* If the container classes seem not to be defined at this stage, then don't try to expand the function */

if ((pp != NULL) && !IsDefinedClass(pp->classes))
   {
   return rval;
   }

ClearFnCallStatus();

expargs = NewExpArgs(fp,pp);

if (UnresolvedArgs(expargs))
   {
   FNCALL_STATUS.status = FNCALL_FAILURE;
   rval.item = CopyFnCall(fp);
   rval.rtype = CF_FNCALL;
   DeleteExpArgs(expargs);
   return rval;
   }

if (this)
   {
   rval = CallFunction(this, fp, expargs);
   }
else
   {
   CfOut(cf_error,"","Un-registered function call");
   PromiseRef(cf_error,pp);
   }

if (FNCALL_STATUS.status == FNCALL_FAILURE)
   {
   /* We do not assign variables to failed function calls */
   rval.item = CopyFnCall(fp);
   rval.rtype = CF_FNCALL;
   }

DeleteExpArgs(expargs);
return rval;
}
                
/*******************************************************************/

FnCallType *FindFunction(const char *name)
{
int i;

for (i = 0; CF_FNCALL_TYPES[i].name != NULL; i++)
   {
   if (strcmp(CF_FNCALL_TYPES[i].name,name) == 0)
      {
      return CF_FNCALL_TYPES + i;
      }
   }
return NULL;
}

/*****************************************************************************/

static void ClearFnCallStatus(void)

{
FNCALL_STATUS.status = CF_NOP;
FNCALL_STATUS.message[0] = '\0';
FNCALL_STATUS.fncall_classes[0] = '\0';
}

/*****************************************************************************/

void SetFnCallReturnStatus(char *name,int status,char *message,char *fncall_classes)

{
FNCALL_STATUS.status = status;

if (message && strlen(message) > 0)
   {
   strncpy(FNCALL_STATUS.message,message,CF_BUFSIZE-1);
   }

if (fncall_classes && strlen(fncall_classes))
   {
   strncpy(FNCALL_STATUS.fncall_classes,fncall_classes,CF_BUFSIZE-1);
   AddPrefixedClasses(name,fncall_classes);
   }
}
