

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
/* File: files_links.c                                                       */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

int VerifyLink(char *destination,char *source,struct Attributes attr,struct Promise *pp)

{ char to[CF_BUFSIZE],linkbuf[CF_BUFSIZE],saved[CF_BUFSIZE],absto[CF_BUFSIZE],*lastnode;
  int nofile = false;
  struct stat sb;
      
Debug("Linkfiles(%s -> %s)\n",destination,source);

lastnode = ReadLastNode(destination);

if (MatchRlistItem(attr.link.copy_patterns,lastnode))
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Link %s matches copy_patterns\n",destination);
   CfLog(cfverbose,OUTPUT,"");
   VerifyCopy(source,destination,attr,pp);
   return true;
   }

memset(to,0,CF_BUFSIZE);
  
if ((*source != '/') && (*source != '.'))  /* links without a directory reference */
   {
   snprintf(to,CF_BUFSIZE-1,"./%s",source);
   }
else
   {
   strncpy(to,source,CF_BUFSIZE-1);
   }

if (*to != '/')         /* relative path, must still check if exists */
   {
   Debug("Relative link destination detected: %s\n",to);
   strcpy(absto,AbsLinkPath(destination,to));
   Debug("Absolute path to relative link = %s, destination %s\n",absto,destination);
   }
else
   {
   strcpy(absto,to);
   }

Debug("Check for source (kill old=%d)\n",attr.link.source);

if (stat(absto,&sb) == -1)
   {
   Debug("No source file\n");
   nofile = true;
   }

if (nofile && attr.link.when_no_file != cfa_force && attr.link.when_no_file != cfa_delete)
   {
   Debug("Returning since the destination is absent\n");
   snprintf(OUTPUT,CF_BUFSIZE-1,"Source for linking is absent");
   return false;  /* no error warning, since the higher level routine uses this */   
   }

if (nofile && attr.link.when_no_file == cfa_delete)
   {
   KillGhostLink(destination,attr,pp);
   return true;
   }
    
/* Move existing object out of way? */

if (!MoveObstruction(destination,attr,pp))
   {
   return false;
   }

memset(linkbuf,0,CF_BUFSIZE);

if (readlink(destination,linkbuf,CF_BUFSIZE-1) == -1)
   {
   if (!MakeParentDirectory(destination,attr.move_obstructions))                  /* link doesn't exist */
      {
      snprintf(OUTPUT,CF_BUFSIZE*2,"Couldn't build directory tree up to %s!\n",destination);
      CfLog(cfsilent,OUTPUT,"");
      snprintf(OUTPUT,CF_BUFSIZE*2,"One element was a plain file, not a directory!\n");
      CfLog(cfsilent,OUTPUT,"");      
      return(true);
      }
   else
      { 
      return MakeLink(destination,to,attr,pp);
      }
   }
else
   { int off1 = 0, off2 = 0;

   /* Link exists */
   
   DeleteSlash(linkbuf);
   
   if (strncmp(linkbuf,"./",2) == 0)   /* Ignore ./ at beginning */
      {
      off1 = 2;
      }
   
   if (strncmp(to,"./",2) == 0)
      {
      off2 = 2;
      }
   
   if (strcmp(linkbuf+off1,to+off2) != 0)
      {
      if (attr.move_obstructions)
         {
         if (!DONTDO)
            {
            snprintf(OUTPUT,CF_BUFSIZE*2,"Overriding incorrect link %s\n",destination);
            CfLog(cfinform,OUTPUT,"");
            ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
            
            if (unlink(destination) == -1)
               {
               perror("unlink");
               return true;
               }
            
            return MakeLink(destination,to,attr,pp);
            }
         else
            {
            snprintf(OUTPUT,CF_BUFSIZE*2,"Must remove incorrect link %s\n",destination);
            CfLog(cferror,OUTPUT,"");
            return false;
            }
         }
      else
         {
         snprintf(OUTPUT,CF_BUFSIZE*2,"Link %s points to %s not %s - not authorized to override",destination,linkbuf,to);
         CfLog(cfinform,OUTPUT,"");
         ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
         return true;
         }
      }

   return true;
   }
}

/*****************************************************************************/

int VerifyAbsoluteLink(char *destination,char *source,struct Attributes attr,struct Promise *pp)

{ char absto[CF_BUFSIZE];
  char expand[CF_BUFSIZE];
  char linkto[CF_BUFSIZE];
  
Debug("VerifyAbsoluteLink(%s,%s)\n",destination,source);

if (*source == '.')
   {
   strcpy(linkto,destination);
   ChopLastNode(linkto);
   AddSlash(linkto);
   strcat(linkto,source);
   }
else
   {
   strcpy(linkto,source);
   }

CompressPath(absto,linkto);

expand[0] = '\0';

if (attr.link.when_no_file == cfa_force)
   {  
   if (!ExpandLinks(expand,absto,0))  /* begin at level 1 and beam out at 15 */
      {
      CfLog(cferror,"Failed to make absolute link in\n","");
      snprintf(OUTPUT,CF_BUFSIZE*2,"%s -> %s\n",destination,source);
      CfLog(cferror,OUTPUT,"");
      return false;
      }
   else
      {
      Debug2("ExpandLinks returned %s\n",expand);
      }
   }
else
   {
   strcpy(expand,absto);
   }

CompressPath(linkto,expand);

return VerifyLink(destination,linkto,attr,pp);
}

/*****************************************************************************/

int VerifyRelativeLink(char *destination,char *source,struct Attributes attr,struct Promise *pp)

{ char *sp, *commonto, *commonfrom;
  char buff[CF_BUFSIZE],linkto[CF_BUFSIZE];
  int levels=0;
  
Debug("RelativeLink(%s,%s)\n",destination,source);

if (*source == '.')
   {
   return VerifyLink(destination,source,attr,pp);
   }

if (!CompressPath(linkto,source))
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Failed to link %s to %s\n",destination,source);
   CfLog(cferror,OUTPUT,"");
   return false;
   }

commonto = linkto;
commonfrom = destination;

if (strcmp(commonto,commonfrom) == 0)
   {
   CfLog(cferror,"Can't link file to itself!\n","");
   snprintf(OUTPUT,CF_BUFSIZE*2,"(%s -> %s)\n",destination,source);
   CfLog(cferror,OUTPUT,"");
   return false;
   }

while (*commonto == *commonfrom)
   {
   commonto++;
   commonfrom++;
   }

while (!((*commonto == '/') && (*commonfrom == '/')))
   {
   commonto--;
   commonfrom--;
   }

commonto++; 

for (sp = commonfrom; *sp != '\0'; sp++)
   {
   if (*sp == '/')
       {
       levels++;
       }
   }

memset(buff,0,CF_BUFSIZE);

strcat(buff,"./");

while(--levels > 0)
   {
   if (BufferOverflow(buff,"../"))
      {
      return false;
      }
   
   strcat(buff,"../");
   }

if (BufferOverflow(buff,commonto))
   {
   return false;
   }

strcat(buff,commonto);
 
return VerifyLink(destination,buff,attr,pp);
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int KillGhostLink(char *name,struct Attributes attr,struct Promise *pp)

{ char linkbuf[CF_BUFSIZE],tmp[CF_BUFSIZE];
  char linkpath[CF_BUFSIZE],*sp;
  struct stat statbuf;

Debug("KillGhostLink(%s)\n",name);

memset(linkbuf,0,CF_BUFSIZE);
memset(linkpath,0,CF_BUFSIZE); 

if (readlink(name,linkbuf,CF_BUFSIZE-1) == -1)
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"(Can't read link %s while checking for deadlinks)\n",name);
   CfLog(cfverbose,OUTPUT,"");
   return true; /* ignore */
   }

if (linkbuf[0] != '/')
   {
   strcpy(linkpath,name);    /* Get path to link */

   for (sp = linkpath+strlen(linkpath); (*sp != '/') && (sp >= linkpath); sp-- )
     {
     *sp = '\0';
     }
   }

strcat(linkpath,linkbuf);
CompressPath(tmp,linkpath); 
 
if (stat(tmp,&statbuf) == -1)               /* link points nowhere */
   {
   if (attr.link.when_no_file == cfa_delete || attr.recursion.rmdeadlinks)
      {

      printf ("%d %d\n",attr.link.when_no_file == cfa_delete,attr.recursion.rmdeadlinks);
      snprintf(OUTPUT,CF_BUFSIZE*2,"%s is a link which points to %s, but that file doesn't seem to exist\n",name,VBUFF);
      CfLog(cfverbose,OUTPUT,"");

      if (! DONTDO)
         {
         unlink(name);  /* May not work on a client-mounted system ! */
         snprintf(OUTPUT,CF_BUFSIZE*2,"Removing ghost %s - reference to something that is not there\n",name);
         CfLog(cfinform,OUTPUT,"");
         ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
         return true;
         }
      }
   }

return false;
}

/*****************************************************************************/

int MakeLink (char *from,char *to,struct Attributes attr,struct Promise *pp)

{
if (DONTDO)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"Need to link files %s -> %s\n",from,to);
   CfLog(cferror,OUTPUT,"");
   return false;
   }
else
   {
   if (symlink(to,from) == -1)
      {
      snprintf(OUTPUT,CF_BUFSIZE*2,"Couldn't link %s to %s\n",to,from);
      CfLog(cferror,OUTPUT,"symlink");
      ClassAuditLog(pp,attr,OUTPUT,CF_FAIL);
      return false;
      }
   else
      {
      snprintf(OUTPUT,CF_BUFSIZE*2,"Linked files %s -> %s\n",from,to);
      CfLog(cfinform,OUTPUT,"");
      ClassAuditLog(pp,attr,OUTPUT,CF_CHG);
      return true;
      }
   }
}
