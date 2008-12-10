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

void CfHtmlHeader(FILE *fp,char *title,char *css,char *webdriver,char *banner)

{
 
fprintf(fp,"<html>"
        "  <head>"
        "    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\" />"
        "    <title>"
        "      %s"
        "    </title>"
        "    <link rel=\"stylesheet\" href=\"%s\" type=\"text/css\" media=\"screen\" />"

        "<SCRIPT TYPE=\"text/javascript\">"
        "function popup(mylink, windowname)"
        "{"
        "if (! window.focus)return true;"
        "var href;"
        "if (typeof(mylink) == \'string\')"
        "   href=mylink;"
        "else"
        "   href=mylink.href;"
        "window.open(href, windowname, \'scrollbars=yes\');"
        "return false;"
        "}"
        "</SCRIPT>"
        "  </head>"
        "  <body>"
        "<div id=\"logo\"><img src=\"cfknow.png\">",title,css);

if (strlen(webdriver) > 0)
   {
   fprintf(fp,"<form action=\"%s\" method=\"post\">",webdriver);
   fprintf(fp,"<div id=\"in\">Search: <input type=\"text\" name=\"regex\" size=\"20\" /></div>");
   fprintf(fp,"</form>");
   }

if (strlen(banner) > 0)
   {
   fprintf(fp,"<div id=\"banner\">%s</div>\n",banner);
   }

fprintf(fp,"</div><div id=\"wholebody\">\n");

fprintf(fp,"<div id=\"title\"><h1>%s</h1></div>",title);
}

/*****************************************************************************/

void CfHtmlFooter(FILE *fp)

{
fprintf(fp,"</div></body></html>\n");
}

