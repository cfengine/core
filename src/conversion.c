/*****************************************************************************/
/*                                                                           */
/* File: conversion.c                                                        */
/*                                                                           */
/* Created: Sun Sep 30 11:26:04 2007                                         */
/*                                                                           */
/* Author:                                           >                       */
/*                                                                           */
/* Revision: $Id$                                                            */
/*                                                                           */
/* Description:                                                              */
/*                                                                           */
/*****************************************************************************/


#include "cf3.defs.h"
#include "cf3.extern.h"


/****************************************************************************/

enum cfsbundle Type2Cfs(char *name)

{ int i;
 
for (i = 0; i < (int)cfs_nobtype; i++)
   {
   if (strcmp(CF_REMACCESS_SUBTYPES[i].subtype,name)==0)
      {
      break;
      }
   }

return (enum cfsbundle)i;
}

/****************************************************************************/

enum cfdatatype Typename2Datatype(char *name)

/* convert abstract data type names: int, ilist etc */
    
{ int i;

Debug("typename2type(%s)\n",name);
 
for (i = 0; i < (int)cf_notype; i++)
   {
   if (strcmp(CF_DATATYPES[i],name)==0)
      {
      break;
      }
   }

return (enum cfdatatype)i;
}

/****************************************************************************/

enum cfagenttype Agent2Type(char *name)

/* convert abstract data type names: int, ilist etc */
    
{ int i;

Debug("Agent2Type(%s)\n",name);
 
for (i = 0; i < (int)cf_notype; i++)
   {
   if (strcmp(CF_AGENTTYPES[i],name)==0)
      {
      break;
      }
   }

return (enum cfagenttype)i;
}

/****************************************************************************/

enum cfdatatype GetControlDatatype(char *varname,struct BodySyntax *bp)

{ int i = 0;

for (i = 0; bp[i].range != NULL; i++)
   {
   if (strcmp(bp[i].lval,varname) == 0)
      {
      return bp[i].dtype;
      }
   }

return cf_notype;
}

/****************************************************************************/

int GetBoolean(char *s)

{ struct Item *list = SplitString(CF_BOOL,','), *ip;
 int count = 0;

for (ip = list; ip != NULL; ip=ip->next)
   {
   if (strcmp(s,ip->name) == 0)
      {
      break;
      }

   count++;
   }

DeleteItemList(list);

if (count % 2)
   {
   return false;
   }
else
   {
   return true;
   }
}
