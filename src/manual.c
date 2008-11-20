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
/* File: manual.c                                                            */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

extern char BUILD_DIR[CF_BUFSIZE];

void TexinfoHeader(FILE *fout);
void TexinfoFooter(FILE *fout);
void TexinfoPromiseTypesFor(FILE *fp,char *s);
void TexinfoBodyParts(FILE *fout,struct BodySyntax *bs);
void TexinfoShowRange(FILE *fout,char *s);
void TexinfoSubBodyParts(FILE *fout,struct BodySyntax *bs);

/*****************************************************************************/

void TexinfoManual(char *mandir)

{ char filename[CF_BUFSIZE];
  FILE *fout;
  int i;

snprintf(filename,CF_BUFSIZE-1,"%scf3-Reference.texinfo",BUILD_DIR);

if ((fout = fopen(filename,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","Unable to open %s for writing\n",filename);
   return;
   }

TexinfoHeader(fout);

/* General background */

/* Components */

for (i = 0; CF_ALL_BODIES[i].btype != NULL; i++)
   {
   fprintf(fout,"@node %s\n@chapter %s\n\n",CF_ALL_BODIES[i].btype,CF_ALL_BODIES[i].btype);
   TexinfoPromiseTypesFor(fout,CF_ALL_BODIES[i].btype);
   }

TexinfoFooter(fout);

fclose(fout);
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

void TexinfoHeader(FILE *fout)
{
 fprintf(fout,
         "\\input texinfo @c -*-texinfo-*-\n"
         "@c *********************************************************************\n"
         "@c\n"
         "@c  This is a TEXINFO file. It generates both TEX documentation and\n"
         "@c  the \"on line\" documentation \"info\" files.\n"
         "@c\n"
         "@c  The file is structured like a programming language. Each chapter\n"
         "@c  starts with a chapter comment.\n"
         "@c\n"
         "@c  Menus list the subsections so that an online info-reader can parse\n"
         "@c  the file hierarchically.\n"
         "@c\n"
         "@c ***********************************************************************\n"
         "@c %** start of header\n"
         "@setfilename cf-Modularize.info\n"
         "@settitle Modularization in cfengine\n"
         "@setchapternewpage odd\n"
         "@c %** end of header\n"
         "@titlepage\n"
         "@title Cfengine Reference Manual\n"
         "@subtitle cfengine documentation\n"
         "@author cfengine.com\n"
         "@c @smallbook\n"
         "@fonttextsize 10\n"
         "@page\n"
         "@vskip 0pt plus 1filll\n"
         "Copyright @copyright{} 2008 Cfengine AS\n"
         "@end titlepage\n"
         "@c *************************** File begins here ************************\n"
         "@ifinfo\n"
         "@dircategory Cfengine Training\n"
         "@direntry\n"
         "* cfengine Modularization:\n"
         "                        Cfengine is a language based framework\n"
         "                        designed for configuring and maintaining\n"
         "                        Unix-like operating systems attached\n"
         "                        to a TCP/IP network.\n"
         "@end direntry\n"
         "@end ifinfo\n"
         
         "@ifnottex\n"
         "@node Top, Modularization, (dir), (dir)\n"
         "@top Cfengine-Modularization\n"
         "@end ifnottex\n"
         
         "@ifhtml\n"
         "@html\n"
         "<a href=\"#Contents\"><h1>COMPLETE TABLE OF CONTENTS</h1></a>\n"
         
         "<h2>Summary of contents</h2>\n"
         
         "@end html\n"
         "@end ifhtml\n"
         );

}

/*****************************************************************************/

void TexinfoFooter(FILE *fout)

{
 fprintf(fout,
         "@c =========================================================================\n"
         "@c @node Index,  , Cfengine Methods, Top\n"
         "@c @unnumbered Concept Index\n"
         "@c @printindex cp\n"
         "@c =========================================================================\n"
         
         "@ifhtml\n"
         "@html\n"
         "<a name=\"Contents\">\n"
         "@end html\n"
         "@end ifhtml\n"
         
         "@contents\n"
         
         "@ifhtml\n"
         "@html\n"
         "<script type=\"text/javascript\">\n"
         "var gaJsHost = ((\"https:\" == document.location.protocol) ? \"https://ssl.\" : \"http://www.\");\n"
         "document.write(unescape(\"%3Cscript src='\" + gaJsHost + \"google-analytics.com/ga.js' type='text/javascript'%%3E%%3C/script%%3E\"));\n"
         "</script>\n"
         "<script type=\"text/javascript\">\n"
         "var pageTracker = _gat._getTracker(\"UA-2576171-2\");\n"
         "pageTracker._initData();\n"
         "pageTracker._trackPageview();\n"
         "</script>\n"
         "@end html\n"
         "@end ifhtml\n"
         "@bye\n"
         );
}

/*****************************************************************************/

void TexinfoPromiseTypesFor(FILE *fout,char *s)

{ int i,j;
  struct SubTypeSyntax *st;

for (i = 0; i < CF3_MODULES; i++)
   {
   st = CF_ALL_SUBTYPES[i];

   for (j = 0; st[j].btype != NULL; j++)
      {
      if (strcmp(s,st[j].btype) == 0 || strcmp("*",st[j].btype) == 0)
         {
         fprintf(fout,"\n\n@node\n@section %s\n\n",st[j].subtype);
         TexinfoBodyParts(fout,st[j].bs);
         }
      }
   }
}


/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

void TexinfoBodyParts(FILE *fout,struct BodySyntax *bs)

{ int i;

if (bs == NULL)
   {
   return;
   }
 
for (i = 0; bs[i].lval != NULL; i++)
   {
   if (bs[i].range == (void *)CF_BUNDLE)
      {
      fprintf(fout,"\n\n@node\n@subsection %s\n\nType: %s (Separate Bundle) \n",bs[i].lval,CF_DATATYPES[bs[i].dtype]);
      }
   else if (bs[i].dtype == cf_body)
      {
      fprintf(fout,"\n\n@node\n@subsection %s\n@noindent Type: %s\n\n",bs[i].lval,CF_DATATYPES[bs[i].dtype]);
      TexinfoSubBodyParts(fout,(struct BodySyntax *)bs[i].range);
      }
   else
      {
      fprintf(fout,"\n\n@node\n@subsection %s\n@noindent Type: %s\n\n",bs[i].lval,CF_DATATYPES[bs[i].dtype]);
      TexinfoShowRange(fout,(char *)bs[i].range);
      fprintf(fout,"@noindent Synopsis: %s\n\n",bs[i].description);
      }
   }
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

void TexinfoShowRange(FILE *fout,char *s)

{ char *sp;
 
if (strlen(s) == 0)
   {
   fprintf(fout,"@noindent Allowed input range: (arbitrary string)\n\n");
   return;
   }

fprintf(fout,"@noindent Allowed input range: %s\n\n",s);
}

/*****************************************************************************/

void TexinfoSubBodyParts(FILE *fout,struct BodySyntax *bs)

{ int i;

if (bs == NULL)
   {
   return;
   }

fprintf(fout,"@table @samp\n");

for (i = 0; bs[i].lval != NULL; i++)
   {
   if (bs[i].range == (void *)CF_BUNDLE)
      {
      fprintf(fout,"@item %s\nType: %s\n (Separate Bundle) \n",bs[i].lval,CF_DATATYPES[bs[i].dtype]);
      }
   else if (bs[i].dtype == cf_body)
      {
      fprintf(fout,"@item %s\nType: %s\n",bs[i].lval,CF_DATATYPES[bs[i].dtype]);
      TexinfoSubBodyParts(fout,(struct BodySyntax *)bs[i].range);
      }
   else
      {
      fprintf(fout,"@item %s\nType: %s\n",bs[i].lval,CF_DATATYPES[bs[i].dtype]);
      TexinfoShowRange(fout,(char *)bs[i].range);
      fprintf(fout," %s\n",bs[i].description);
      }
   }

fprintf(fout,"@end table\n");
}

