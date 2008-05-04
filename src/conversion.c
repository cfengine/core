/* 
   Copyright (C) 2008 - Mark Burgess

   This file is part of Cfengine 3 - written and maintained by Mark Burgess.
 
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
/* File: conversion.c                                                        */
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

/****************************************************************************/

int Str2Int(char *s)

{ int a = CF_NOINT;

if (s == NULL)
   {
   return CF_NOINT;
   }
 
sscanf(s,"%d",&a);
 
if (a == CF_NOINT)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Error reading assumed integer value %s\n",s);
   ReportError(OUTPUT);
   }

return a;
}

/****************************************************************************/

int Str2Double(char *s)

{ double a = CF_NODOUBLE;
 
if (s == NULL)
   {
   return CF_NODOUBLE;
   }

sscanf(s,"%d",&a);
 
if (a == CF_NODOUBLE)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Error reading assumed real value %s\n",s);
   ReportError(OUTPUT);
   }

return a;
}
