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

{ struct CfRegEx rex;
 
rex = CompileRegExp(regexp);

if (rex.failed)
   {
   return 0;
   }

if (RegExMatchFullString(rex,teststring))
   {
   return true;
   }
else
   {
   return false;
   }
}

/*************************************************************************/

int BlockTextMatch(char *regexp,char *teststring,int *start,int *end)

{ struct CfRegEx rex;
 
rex = CompileRegExp(regexp);

if (rex.failed)
   {
   return 0;
   }

if (RegExMatchSubString(rex,teststring,start,end))
   {
   return true;
   }
else
   {
   return false;
   } 
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
  int result = false,s = 0,r = 0;

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
                CfOut(cf_error,"","Path regular expression %s seems to use expressions containing the directory symbol %c",str,FILE_SEPARATOR);
                CfOut(cf_error,"","Use a work-around to avoid pathological behaviour\n");
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

int MatchRlistItem(struct Rlist *listofregex,char *teststring)

   /* Checks whether item matches a list of wildcards */

{ struct Rlist *rp;
 
for (rp = listofregex; rp != NULL; rp=rp->next)
   {
   /* Make it commutative */
   
   if (FullTextMatch(rp->item,teststring) || FullTextMatch(teststring,rp->item))
      {
      Debug("MatchRlistItem(%s > %s)\n",rp->item,teststring);
      return true;
      }
   }

return false;
}


/*********************************************************************/
/* Wrappers                                                          */
/*********************************************************************/

struct CfRegEx CompileRegExp(char *regexp)

{ struct CfRegEx this;
 
#ifdef HAVE_LIBPCRE
 pcre *rx;
 const char *errorstr; 
 int erroffset;

memset(&this,0,sizeof(struct CfRegEx)); 
rx = pcre_compile(regexp,0,&errorstr,&erroffset,NULL);

if (rx == NULL)
   {
   CfOut(cf_error,"","Regular expression error %s in %s at %d: %s\n",errorstr,regexp,erroffset);
   this.failed = true;
   }
else
   {
   this.failed = false;
   this.rx = rx;
   }

#else

 regex_t rx;
 int code;

memset(&this,0,sizeof(struct CfRegEx)); 
re_syntax_options |= RE_INTERVALS;

code = regcomp(&rx,regexp,REG_EXTENDED);

if (code != 0)
   {
   char buf[CF_BUFSIZE];
   regerror(code,&rx,buf,CF_BUFSIZE-1);
   CfOut(cf_error,"regerror","Regular expression error %d for %s: %s\n", code, regexp,buf);
   this.failed = true;
   }
else
   {
   this.failed = false;
   this.rx = rx;
   }

#endif

this.regexp = regexp;
return this;
}

/*********************************************************************/

int RegExMatchSubString(struct CfRegEx rex,char *teststring,int *start,int *end)

{
#ifdef HAVE_LIBPCRE
 pcre *rx;
 int ovector[OVECCOUNT],i,rc;
 
rx = rex.rx;

if ((rc = pcre_exec(rx,NULL,teststring,strlen(teststring),0,0,ovector,OVECCOUNT)) >= 0)
   {
   *start = ovector[0];
   *end = ovector[1];
   return true;
   }
else
   {
   *start = 0;
   *end = 0;
   return false;
   }

#else

 regex_t rx = rex.rx;
 regmatch_t pmatch;
 int code;
 
if ((code = regexec(&rx,teststring,1,&pmatch,0)) == 0)
   {
   *start = pmatch.rm_so;
   *end = pmatch.rm_eo;
   return true;
   }
else
   {
   char buf[CF_BUFSIZE];
   regerror(code,&rx,buf,CF_BUFSIZE-1);
   Debug("Regular expression error %d for %s: %s\n", code,rex.regexp,buf);
   *start = 0;
   *end = 0;
   return false;
   }

#endif
}

/*********************************************************************/

int RegExMatchFullString(struct CfRegEx rex,char *teststring)

{
#ifdef HAVE_LIBPCRE
 pcre *rx;
 int ovector[OVECCOUNT],i,rc;
 
rx = rex.rx;

if ((rc = pcre_exec(rx,NULL,teststring,strlen(teststring),0,0,ovector,OVECCOUNT)) >= 0)
   {
   for (i = 0; i < rc; i++)
      {
      char substring[1024];
      char *match_start = teststring + ovector[i*2];
      int match_len = ovector[i*2+1] - ovector[i*2];
      memset(substring,0,1024);
      strncpy(substring,match_start,match_len);
      
      if ((match_start == teststring) && (match_len == strlen(teststring)))
         {
         return true;
         }
      else
         {
         return false;
         }
      }
   }
else
   {
   return false;
   }

#else

 regex_t rx = rex.rx;
 regmatch_t pmatch;
 int code;
 
if ((code = regexec(&rx,teststring,1,&pmatch,0)) == 0)
   {
   if ((pmatch.rm_so == 0) && (pmatch.rm_eo == strlen(teststring)))
      {
      Debug("Regex %s matches (%s) exactly.\n",rex.regexp,teststring);
      return true;
      }
   else
      {
      Debug("Regex %s did not match (%s) exactly, but it matched a part of it.\n",rex.regexp,teststring);
      return false;
      }
   }
else
   {
   char buf[CF_BUFSIZE];
   regerror(code,&rx,buf,CF_BUFSIZE-1);
   Debug("Regular expression error %d for %s: %s\n", code,rex.regexp,buf);
   return false;
   }

#endif
return false;
}

/* EOF */
