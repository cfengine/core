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
/* File: matching.c                                                          */
/*                                                                           */
/*****************************************************************************/
 
#include "cf3.defs.h"
#include "cf3.extern.h"

/*************************************************************************/
/* WILDCARD TOOLKIT : Level 0                                            */
/*************************************************************************/

int FullTextMatch (char *regptr,char *cmpptr)

{ regex_t rx,rxcache;
  regmatch_t pmatch;
  char regexp[2048]; 
  char teststring[2048];
  int code;
  char buf[1024];
  
re_syntax_options |= RE_INTERVALS;

code = regcomp(&rx,regexp,REG_EXTENDED);

if (code != 0)
   {
   regerror(code,&rx,buf,1023);
   snprintf(OUTPUT,CF_BUFSIZE,"Regular expression error %d for %s: %s\n", code, regexp,buf);
   CfLog(cferror,OUTPUT,"regerror");
   return 0;
   }
else
   {
   if ((code =regexec(&rx,teststring,1,&pmatch,0)) == 0)
      {
      if ((pmatch.rm_so == 0) && (pmatch.rm_eo == strlen(teststring)))
         {
         Debug("Matched\n");
         return true;
         }
      else
         {
         Debug("Regex %s did not match (%s) exactly, but it matched a part of it.\n",regexp,teststring);
         }
      }
   else
      {
      regerror(code,&rx,buf,1023);
      Verbose("Regular expression error %d for %s: %s\n", code, regexp,buf);
      }
   }

return false;
}

/*********************************************************************/

int IsRegexItemIn(struct Item *list,char *regex)

   /* Checks whether item matches a list of wildcards */

{ struct Item *ptr;
 
for (ptr = list; ptr != NULL; ptr=ptr->next)
   {
   if (IsExcluded(ptr->classes))
      {
      continue;
      }

   /* Make it commutative */
   
   if (FullTextMatch(regex,ptr->name) || FullTextMatch(ptr->name,regex))
      {
      Debug("IsRegexItem(%s,%s)\n",regex,ptr->name);
      return(true);
      }
   }

return(false);
}


/* EOF */
