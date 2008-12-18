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
/* File: html.c                                                              */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void CfHtmlHeader(FILE *fp,char *title,char *css,char *webdriver,char *header)

{
fprintf(fp,"<html>"
        "  <head>"
        "    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\" />"
        "    <title>"
        "      %s"
        "    </title>"
        "    <link rel=\"stylesheet\" href=\"%s\" type=\"text/css\" media=\"screen\" />"
        "  </head>"
        "  <body>",title,css);

if (strlen(header) > 0)
   {
   fprintf(fp,"%s\n",header);
   }


fprintf(fp,"<h1>%s</h1>",title);
}

/*****************************************************************************/

void CfHtmlFooter(FILE *fp,char *footer)

{
 /* This section should be conditional */

 if (strlen(footer) > 0)
    {
    fprintf(fp,"%s",footer);
    }
/* end */

fprintf(fp,"</div></body></html>\n");
}
