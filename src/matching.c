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
/* File: matching.c                                                          */
/*                                                                           */
/*****************************************************************************/
 
#include "cf3.defs.h"
#include "cf3.extern.h"

/*************************************************************************/
/* WILDCARD TOOLKIT : Level 0                                            */
/*************************************************************************/

int FullTextMatch (char *regexp,char *teststring)

{ regex_t rx,rxcache;
  regmatch_t pmatch;
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
         Debug("Regex %s matches (%s) exactly.\n",regexp,teststring);
         return true;
         }
      else
         {
         Debug("Regex %s did not match (%s) exactly, but it matched a part of it.\n",regexp,teststring);
         return false;
         }
      }
   else
      {
      regerror(code,&rx,buf,1023);
      Debug("Regular expression error %d for %s: %s\n", code, regexp,buf);
      return false;
      }
   }

return false;
}

/*********************************************************************/

int IsRegex(char *str)

{ char *sp;

for (sp = str; *sp != '\0'; sp++)
   {
   if (strchr("^*+\[-]()$",*sp))
      {
      return true;  /* Maybe */
      }
   }

return false;
}

/*********************************************************************/

int IsPathRegex(char *str)

{ char *sp;
  int result,s = 0,r = 0;

if (result = IsRegex(str))
   {
   for (sp = str; *sp != '\0'; sp++)
      {
      switch(*sp)
         {
         case '[':
             s++;
             break;
         case ']':
             s--;
             break;
         case '(':
             r++;
             break;
         case')':
             r--;
             break;
         case FILE_SEPARATOR:
             
             if (r || s)
                {
                snprintf(OUTPUT,CF_BUFSIZE,"Path regular expression %s seems to use expressions containing the directory symbol %c",str,FILE_SEPARATOR);
                CfLog(cferror,OUTPUT,"");
                CfLog(cferror,"Use a work-around to avoid pathological behaviour\n","");
                return false;
                }
             break;
         }
      }
   }

return result;
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

/*********************************************************************/

/* EOF */
