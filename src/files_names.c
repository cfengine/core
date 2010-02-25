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
/* File: files_names.c                                                       */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void LocateFilePromiserGroup(char *wildpath,struct Promise *pp,void (*fnptr)(char *path, struct Promise *ptr))

{ struct Item *path,*ip,*remainder = NULL;
  char pbuffer[CF_BUFSIZE];
  struct stat statbuf;
  int count = 0,lastnode = false, expandregex = false;
  uid_t agentuid = getuid();
  int create = GetBooleanConstraint("create",pp);

Debug("LocateFilePromiserGroup(%s)\n",wildpath);

/* Do a search for promiser objects matching wildpath */

if (!IsPathRegex(wildpath))
   {
   CfOut(cf_verbose,""," -> Using literal pathtype for %s\n",wildpath);
   (*fnptr)(wildpath,pp);
   return;
   }
else
   {
   CfOut(cf_verbose,""," -> Using regex pathtype for %s (see pathtype)\n",wildpath);
   }

pbuffer[0] = '\0';
path = SplitString(wildpath,'/');  // require forward slash in regex on all platforms

for (ip = path; ip != NULL; ip=ip->next)
   {
   if (ip->name == NULL || strlen(ip->name) == 0)
      {
      continue;
      }

   if (ip->next == NULL)
      {
      lastnode = true;
      }

   /* No need to chdir as in recursive descent, since we know about the path here */
   
   if (IsRegex(ip->name))
      {
      remainder = ip->next;
      expandregex = true;
      break;
      }
   else
      {
      expandregex = false;
      }

   if (!JoinPath(pbuffer,ip->name))
      {
      CfOut(cf_error,"","Buffer overflow in LocateFilePromiserGroup\n");
      return;
      }

   if (cfstat(pbuffer,&statbuf) != -1)
      {
      if (S_ISDIR(statbuf.st_mode) && statbuf.st_uid != agentuid && statbuf.st_uid != 0)
         {
         CfOut(cf_inform,"","Directory %s in search path %s is controlled by another user (uid %d) - trusting its content is potentially risky (possible race)\n",pbuffer,wildpath,statbuf.st_uid);
         PromiseRef(cf_inform,pp);
         }
      }
   }

if (expandregex) /* Expand one regex link and hand down */
   {
   char nextbuffer[CF_BUFSIZE],nextbufferOrig[CF_BUFSIZE],regex[CF_BUFSIZE];
   struct dirent *dirp;
   DIR *dirh;
   struct Attributes dummyattr;

   memset(&dummyattr,0,sizeof(dummyattr));
   memset(regex,0,CF_BUFSIZE);

   strncpy(regex,ip->name,CF_BUFSIZE-1);

   if ((dirh=opendir(pbuffer)) == NULL)
      {
      // Could be a dummy directory to be created so this is not an error.
      CfOut(cf_verbose,""," -> Using best-effort expanded (but non-existent) file base path %s\n",wildpath);
      (*fnptr)(wildpath,pp);
      DeleteItemList(path);
      return;
      }
   else
      {
      count = 0;
   
      for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh))
         {
         if (!ConsiderFile(dirp->d_name,pbuffer,dummyattr,pp))
            {
            continue;
            }
         
         if (!lastnode && !S_ISDIR(statbuf.st_mode))
            {
            Debug("Skipping non-directory %s\n",dirp->d_name);
            continue;
            }
         
         if (FullTextMatch(regex,dirp->d_name))
            {
            Debug("Link %s matched regex %s\n",dirp->d_name,regex);
            }
         else
            {
            continue;
            }
         
         count++;
         
         strncpy(nextbuffer,pbuffer,CF_BUFSIZE-1);
         AddSlash(nextbuffer);
         strcat(nextbuffer,dirp->d_name);
         
         for (ip = remainder; ip != NULL; ip=ip->next)
            {
            AddSlash(nextbuffer);
            strcat(nextbuffer,ip->name);
            }
         
         /* The next level might still contain regexs, so go again as long as expansion is not nullpotent */
         
         if (!lastnode && (strcmp(nextbuffer,wildpath) != 0))
            {
            LocateFilePromiserGroup(nextbuffer,pp,fnptr);
            }
         else
            {
            struct Promise *pcopy;
            CfOut(cf_verbose,""," -> Using expanded file base path %s\n",nextbuffer);

            /* Now need to recompute any back references to get the complete path */

	    snprintf(nextbufferOrig, sizeof(nextbufferOrig), "%s", nextbuffer);
	    MapNameForward(nextbuffer);

            if (!FullTextMatch(pp->promiser,nextbuffer))
               {
               CfOut(cf_error,"","Error recomputing references");
               }

            /* If there were back references there could still be match.x vars to expand */

            pcopy = ExpandDeRefPromise(CONTEXTID,pp);            
            (*fnptr)(nextbufferOrig,pcopy);
            DeletePromise(pcopy);
            }
         }
      
      closedir(dirh);
      }
   }
else
   {
   CfOut(cf_verbose,""," -> Using file base path %s\n",pbuffer);
   (*fnptr)(pbuffer,pp);
   }

if (count == 0)
   {
   CfOut(cf_verbose,"","No promiser file objects matched as regular expression %s\n",wildpath);

   if (create)
      {
      VerifyFilePromise(pp->promiser,pp);
      }
   }

DeleteItemList(path);
}

/*********************************************************************/

int IsDir(char *path)
/*
Checks if the object pointed to by path exists and is a directory.
Returns true if so, false otherwise.
*/
{
#ifdef MINGW
return NovaWin_IsDir(path);
#else
struct stat sb;

if (cfstat(path, &sb) != -1)
   {
   if (S_ISDIR(sb.st_mode))
      {
      return true;
      }
   }

return false;
#endif  /* NOT MINGW */
}

/*********************************************************************/

int EmptyString(char *s)

{ char *sp;

for (sp = s; *sp != '\0'; sp++)
   {
   if (!isspace(*sp))
      {
      return false;
      }
   }

return true;
}

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

Chop(path);
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

Chop(path);
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

// add root slash on Unix systems
if(strlen(str) == 0)
  {
  if((VSYSTEMHARDCLASS != mingw) && (VSYSTEMHARDCLASS != cfnt))
    {
    strcpy(str, "/");
    }
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

/*****************************************************************************/

void DeEscapeFilename(char *in,char *out)

{ char *sp_in,*sp_out = out;

*sp_out = '\0';

for (sp_in = in; *sp_in != '\0'; sp_in++)
   {
   if (*sp_in == '\\' && *(sp_in+1) == ' ')
      {
      sp_in++;
      }

   *sp_out++ = *sp_in;
   }

*sp_out = '\0';
}

/*****************************************************************************/

int DeEscapeQuotedString(char *from,char *to)

{ char *sp,*cp;
  char start = *from;
  int len = strlen(from);

if (len == 0)
   {
   return 0;
   }

 for (sp=from+1,cp=to; (sp-from) < len; sp++,cp++)
    {
    if ((*sp == start))
       {
       *(cp) = '\0';

       if (*(sp+1) != '\0')
          {
          return (2+(sp - from));
          }

       return 0;
       }

    if (*sp == '\n')
       {
       P.line_no++;
       }

    if (*sp == '\\')
       {
       switch (*(sp+1))
          {
          case '\n':
              P.line_no++;
              sp+=2;
              break;

          case ' ':
              break;
              
          case '\\':
          case '\"':
          case '\'': sp++;
              break;
          }
       }

    *cp = *sp;    
    }
 
 yyerror("Runaway string");
 *(cp) = '\0';
 return 0;
}

/*********************************************************************/

void Chop(char *str) /* remove trailing spaces */

{ int i;
 
if ((str == NULL) || (strlen(str) == 0))
   {
   return;
   }

if (strlen(str) > CF_EXPANDSIZE)
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

Debug("CompressPath(%s,%s)\n",dest,src);

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

   strncpy(node,sp,nodelen);
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

if (isalpha(f[0]) && f[1] == ':' && IsFileSep(f[2]))
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

{ int len;
  char *sp;

#ifdef NT

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

if (isalpha(f[0]) && f[1] == ':')
   {
   if(IsFileSep(f[2]))
     {
     return 3;
     }

   return 2;  
   }
#endif

if (IsFileSep(*f))
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
if (isupper((int)ch))
   {
   return(ch - 'A' + 'a');
   }
else
   {
   return(ch);
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

/*********************************************************************/

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
void *ThreadUniqueName(pthread_t tid)
/* pthread_t is an integer on Unix, but a structure on Windows
 * Finds a unique name for a thread for both platforms. */
{
#ifdef MINGW
return tid.p;  // pointer to thread structure
#else
return (void*)tid;  // index into thread array ?
#endif  /* NOT MINGW */
}
#endif  /* HAVE PTHREAD */
