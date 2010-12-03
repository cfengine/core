

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
/* File: alphalist.c                                                         */
/*                                                                           */
/* Created: Fri Dec  3 10:26:22 2010                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/
/* This library creates a simple indexed array of lists for optimization of
   high entropy class searches.

 
 struct AlphaList al;
 struct Item *ip;
 int i;

 InitAlphaList(&al);
 PrependAlphaList(&al,"one");
 PrependAlphaList(&al,"two");
  PrependAlphaList(&al,"three");
 PrependAlphaList(&al,"onetwo");
 VERBOSE = 1;   
 ShowAlphaList(al);
 exit(0); */
/*****************************************************************************/

void InitAlphaList(struct AlphaList *al)

{ int i;

for (i = 0; i < 256; i++)
   {
   al->list[i] = NULL;
   }
}

/*****************************************************************************/

void DeleteAlphaList(struct AlphaList *al)

{ int i;

for (i = 0; i < 256; i++)
   {
   DeleteItemList(al->list[i]);
   al->list[i] = NULL;
   }
}

/*****************************************************************************/

int InAlphaList(struct AlphaList *al,char *string)

{ int i = (int)*string;
  
return IsItemIn(al->list[i],string);
}

/*****************************************************************************/

int MatchInAlphaList(struct AlphaList *al,char *string)

{ struct Item *ip;
 int i = (int)*string;

if (isalnum(*string))
   {
   return IsItemIn(al->list[i],string);
   }
else
   {
   // We don't know what the correct hash is because the pattern in vague

   for (ip = al->list[i]; i < 256; i++)
      {
      if (IsItemIn(al->list[i],string))
         {
         return true;
         }
      }
   }

return false;
}

/*****************************************************************************/

void PrependAlphaList(struct AlphaList *al,char *string)

{ int i = (int)*string;


 printf("ADDING %s at %d\n",string,i);
al->list[i] = PrependItem(&(al->list[i]),string,NULL); 
}

/*****************************************************************************/

void ShowAlphaList(struct AlphaList al)

{ int i;
  struct Item *ip;

if (!(VERBOSE||DEBUG))
   {
   return;
   }
  
for (i = 0; i < 256; i++)
   {
   if (al.list[i] == NULL)
      {
      }
   else       
      {
      printf("%c :",(char)i);

      for (ip = al.list[i]; ip != NULL; ip=ip->next)
         {
         printf(" %s",ip->name);
         }

      printf("\n");
      }
   }
}
