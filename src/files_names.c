/* 
   Copyright (C) 2008 - Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
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
/* File: files_names.c                                                       */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*********************************************************************/

int ExpandOverflow(char *str1,char *str2)   /* Should be an inline ! */

{ int len = strlen(str2);

if ((strlen(str1)+len) > (CF_EXPANDSIZE - CF_BUFFERMARGIN))
   {
   CfOut(cf_error,"","Expansion overflow constructing string. Increase CF_EXPANDSIZE macro. Tried to add %s to %s\n",str2,str1);
   return true;
   }

return false;
}

/*********************************************************************/

char *JoinPath(char *path,char *leaf)

{ int len = strlen(leaf);

AddSlash(path);

if ((strlen(path)+len) > (CF_BUFSIZE - CF_BUFFERMARGIN))
   {
   CfOut(cf_error,"","Buffer overflow constructing string. Tried to add %s to %s\n",leaf,path);
   return NULL;
   }

strcat(path,leaf);
return path;
}

/*********************************************************************/

char *JoinSuffix(char *path,char *leaf)

{ int len = strlen(leaf);
 
DeleteSlash(path);
      
if ((strlen(path)+len) > (CF_BUFSIZE - CF_BUFFERMARGIN))
   {
   CfOut(cf_error,"","Buffer overflow constructing string. Tried to add %s to %s\n",leaf,path);
   return NULL;
   }

strcat(path,leaf);
return path;
}

/*********************************************************************/

int IsAbsPath(char *path)

{
if (IsFileSep(*path))
   {
   return true;
   }
else
   {
   return false;
   }
}

/*******************************************************************/

void AddSlash(char *str)

{ char *sp, *sep = FILE_SEPARATOR_STR;
  int f = false ,b = false;

if (str == NULL)
   {
   return;
   }

/* Try to see what convention is being used for filenames
   in case this is a cross-system copy from Win/Unix */

for (sp = str; *sp != '\0'; sp++)
   {
   switch (*sp)
      {
      case '/':
          f = true;
          break;
      case '\\':
          b = true;
          break;
      default:
          break;
      }
   }

if (f && !b)
   {
   sep = "/";
   }
else if (b && !f)
   {
   sep = "\\";
   }

if (!IsFileSep(str[strlen(str)-1]))
   {
   strcat(str,sep);
   }
}

/*********************************************************************/

void DeleteSlash(char *str)

{
if ((strlen(str)== 0) || (str == NULL))
   {
   return;
   }

if (strcmp(str,"/") == 0)
   {
   return;
   }
 
if (IsFileSep(str[strlen(str)-1]))
   {
   str[strlen(str)-1] = '\0';
   }
}

/*********************************************************************/

char *LastFileSeparator(char *str)

  /* Return pointer to last file separator in string, or NULL if 
     string does not contains any file separtors */

{ char *sp;

/* Walk through string backwards */
 
sp = str + strlen(str) - 1;

 while (sp >= str) 
   {
   if (IsFileSep(*sp))
      {
      return sp;
      }
   sp--;
   }

return NULL;
}

/*********************************************************************/

int ChopLastNode(char *str)

  /* Chop off trailing node name (possible blank) starting from
     last character and removing up to the first / encountered 
     e.g. /a/b/c -> /a/b
          /a/b/ -> /a/b                                        */
{ char *sp;
 int ret; 

if ((sp = LastFileSeparator(str)) == NULL)
   {
   ret = false;
   }
else
   {
   *sp = '\0';
   ret = true;
   }

if (strlen(str) == 0)
   {
   AddSlash(str);
   }
 
return ret; 
}

/*********************************************************************/

char *CanonifyName(char *str)

{ static char buffer[CF_BUFSIZE];
  char *sp;

memset(buffer,0,CF_BUFSIZE);
strcpy(buffer,str);

for (sp = buffer; *sp != '\0'; sp++)
    {
    if (!isalnum((int)*sp) || *sp == '.')
       {
       *sp = '_';
       }
    }

return buffer;
}

/*********************************************************************/

char *ReadLastNode(char *str)

/* Return the last node of a pathname string  */

{ char *sp;
  
if ((sp = LastFileSeparator(str)) == NULL)
   {
   return str;
   }
else
   {
   return sp + 1;
   }
}

/*********************************************************************/

void Chop(char *str) /* remove trailing spaces */

{ int i;
 
if ((str == NULL) || (strlen(str) == 0))
   {
   return;
   }

if (strlen(str) > CF_BUFSIZE)
   {
   CfOut(cf_error,"","Chop was called on a string that seemed to have no terminator");
   return;
   }

for (i = strlen(str)-1; isspace((int)str[i]); i--)
   {
   str[i] = '\0';
   }
}


/*********************************************************************/

int CompressPath(char *dest,char *src)

{ char *sp;
  char node[CF_BUFSIZE];
  int nodelen;
  int rootlen;

Debug2("CompressPath(%s,%s)\n",dest,src);

memset(dest,0,CF_BUFSIZE);

rootlen = RootDirLength(src);
strncpy(dest,src,rootlen);
 
for (sp = src+rootlen; *sp != '\0'; sp++)
   {
   if (IsFileSep(*sp))
      {
      continue;
      }

   for (nodelen = 0; sp[nodelen] != '\0' && !IsFileSep(sp[nodelen]); nodelen++)
      {
      if (nodelen > CF_MAXLINKSIZE)
         {
         CfOut(cf_error,"","Link in path suspiciously large");
         return false;
         }
      }

   strncpy(node, sp, nodelen);
   node[nodelen] = '\0';
   
   sp += nodelen - 1;
   
   if (strcmp(node,".") == 0)
      {
      continue;
      }
   
   if (strcmp(node,"..") == 0)
      {
      if (!ChopLastNode(dest))
         {
         Debug("cfengine: used .. beyond top of filesystem!\n");
         return false;
         }
   
      continue;
      }
   else
      {
      AddSlash(dest);
      }

   if (!JoinPath(dest,node))
      {
      return false;
      }
   }
 
return true;
}

/*********************************************************************/

int IsIn(char c,char *str)

{ char *sp;

for(sp = str; *sp != '\0'; sp++)
   {
   if (*sp == c)
      {
      return true;
      }
   }
return false;
}

/*********************************************************************/

int IsAbsoluteFileName(char *f)

{
#ifdef NT
if (IsFileSep(f[0]) && IsFileSep(f[1]))
   {
   return true;
   }
if ( isalpha(f[0]) && f[1] == ':' && IsFileSep(f[2]) )
   {
   return true;
   }
#endif
if (*f == '/')
   {
   return true;
   }

return false;
}


/*******************************************************************/

int RootDirLength(char *f)

  /* Return length of Initial directory in path - */

{
#ifdef NT
  int len;

if (IsFileSep(f[0]) && IsFileSep(f[1]))
   {
   /* UNC style path */

   /* Skip over host name */
   for (len=2; !IsFileSep(f[len]); len++)
      {
      if (f[len] == '\0')
         {
         return len;
         }
      }
   
   /* Skip over share name */
   for (len++; !IsFileSep(f[len]); len++)
      {
      if (f[len] == '\0')
         {
         return len;
         }
      }
   
   /* Skip over file separator */
   len++;
   
   return len;
   }
 if ( isalpha(f[0]) && f[1] == ':' && IsFileSep(f[2]) )
    {
    return 3;
    }
#endif
 if (*f == '/')
    {
    return 1;
    }
 
 return 0;
}

/*********************************************************************/
/* TOOLKIT : String                                                  */
/*********************************************************************/

char ToLower (char ch)

{
if (isdigit((int)ch) || ispunct((int)ch))
   {
   return(ch);
   }

if (islower((int)ch))
   {
   return(ch);
   }
else
   {
   return(ch - 'A' + 'a');
   }
}


/*********************************************************************/

char ToUpper (char ch)

{
if (isdigit((int)ch) || ispunct((int)ch))
   {
   return(ch);
   }

if (isupper((int)ch))
   {
   return(ch);
   }
else
   {
   return(ch - 'a' + 'A');
   }
}

/*********************************************************************/

char *ToUpperStr (char *str)

{ static char buffer[CF_BUFSIZE];
  int i;

memset(buffer,0,CF_BUFSIZE);
  
if (strlen(str) >= CF_BUFSIZE)
   {
   char *tmp;
   tmp = malloc(40+strlen(str));
   sprintf(tmp,"String too long in ToUpperStr: %s",str);
   FatalError(tmp);
   }

for (i = 0;  (str[i] != '\0') && (i < CF_BUFSIZE-1); i++)
   {
   buffer[i] = ToUpper(str[i]);
   }

buffer[i] = '\0';

return buffer;
}


/*********************************************************************/

char *ToLowerStr (char *str)

{ static char buffer[CF_BUFSIZE];
  int i;

memset(buffer,0,CF_BUFSIZE);

if (strlen(str) >= CF_BUFSIZE-1)
   {
   char *tmp;
   tmp = malloc(40+strlen(str));
   snprintf(tmp,CF_BUFSIZE-1,"String too long in ToLowerStr: %s",str);
   FatalError(tmp);
   }

for (i = 0; (str[i] != '\0') && (i < CF_BUFSIZE-1); i++)
   {
   buffer[i] = ToLower(str[i]);
   }

buffer[i] = '\0';

return buffer;
}

