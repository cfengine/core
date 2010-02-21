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
/* File: report.c                                                            */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"


char *CFX[][2] =
   {
    "<head>","</head>",
    "<bundle>","</bundle>",
    "<block>","</block>",
    "<blockheader>","</blockheader>",
    "<blockid>","</blockid>",
    "<blocktype>","</blocktype>",
    "<args>","</args>",
    "<promise>","</promise>",
    "<class>","</class>",
    "<subtype>","</subtype>",
    "<object>","</object>",
    "<lval>","</lval>",
    "<rval>","</rval>",
    "<qstring>","</qstring>",
    "<rlist>","</rlist>",
    "<function>","</function>",
    "\n","\n",
    NULL,NULL
   };

char *CFH[][2] =
   {
    "<html><head>\n<link rel=\"stylesheet\" type=\"text/css\" href=\"/cf_enterprise.css\" />\n</head>\n","</html>",
    "<div id=\"bundle\"><table class=border><tr><td><h2>","</td></tr></h2></table></div>",
    "<div id=\"block\"><table class=border cellpadding=5 width=800>","</table></div>",
    "<tr><th>","</th></tr>",
    "<span class=\"bodyname\">","</span>",
    "<span class=\"bodytype\">","</span>",
    "<span class=\"args\">","</span>",
    "<tr><td><table class=\"border\"><tr><td>","</td></tr></table></td></tr>",
    "<span class=\"class\">","</span>",
    "<span class=\"subtype\">","</span>",
    "<b>","</b>",
    "<br><span class=\"lval\">........................","</span>",
    "<span class=\"rval\">","</span>",
    "<span class=\"qstring\">","</span>",
    "<span class=\"rlist\">","</span>",
    "","",
    "<tr><td>","</td></tr>",
    NULL,NULL
   };

/*******************************************************************/
/* Generic                                                         */
/*******************************************************************/

void ShowContext(void)

{ struct Item *ptr;
  char vbuff[CF_BUFSIZE];

 /* Text output */

CfOut(cf_verbose,"","");

ptr = SortItemListNames(VHEAP);
VHEAP = ptr;

if (VERBOSE||DEBUG)
   {
   snprintf(vbuff,CF_BUFSIZE,"Host %s's basic classified context",VFQNAME);
   ReportBanner(vbuff);
   
   printf("%s  -> Defined classes = { ",VPREFIX);
   
   for (ptr = VHEAP; ptr != NULL; ptr=ptr->next)
      {
      printf("%s ",ptr->name);
      }
   
   printf("}\n");

   CfOut(cf_verbose,"","");
   
   printf("%s  -> Negated Classes = { ",VPREFIX);
   
   for (ptr = VNEGHEAP; ptr != NULL; ptr=ptr->next)
      {
      printf("%s ",ptr->name);
      }
   
   printf ("}\n");
   }

CfOut(cf_verbose,"","");

/* HTML output */

/*
fprintf(FREPORT_HTML,"<div id=\"contextclasses\">");
fprintf(FREPORT_HTML,"<table class=border><tr>");
fprintf(FREPORT_HTML,"<tr><th colspan=2><h1>Agent's hard context classes</h1></th></tr>\n");

fprintf(FREPORT_HTML,"<th>Defined Classes</th><td><ul>");
fprintf(FREPORT_HTML,"<a name=\"class_context\"></a>");

for (ptr = VHEAP; ptr != NULL; ptr=ptr->next)
   {
   fprintf(FREPORT_HTML,"<li>%s%s%s \n",CFH[cfx_class][cfb],ptr->name,CFH[cfx_class][cfe]);
   }

fprintf(FREPORT_HTML,"</ul></td></tr><tr>\n");

fprintf(FREPORT_HTML,"<th>Negated Classes</th><td><ul>");

for (ptr = VNEGHEAP; ptr != NULL; ptr=ptr->next)
   {
   fprintf(FREPORT_HTML,"<li>%s%s%s \n",CFH[cfx_class][cfb],ptr->name,CFH[cfx_class][cfe]);
   }

fprintf(FREPORT_HTML,"</ul></td></tr></table></div><p>\n");
*/
}

/*******************************************************************/

void ShowControlBodies()

{ int i;

printf("<h1>Control bodies for cfengine components</h1>\n");

printf("<div id=\"bundles\">");
printf("<ul>\n");

for (i = 0; CF_ALL_BODIES[i].btype != NULL; i++)
   {
   printf("<li>COMPONENT %s</li>\n", CF_ALL_BODIES[i].btype);

   printf("<li><h4>PROMISE TYPE %s</h4>\n",CF_ALL_BODIES[i].subtype);
   ShowBodyParts(CF_ALL_BODIES[i].bs);
   printf("</li>\n");
   }

printf("</ul></div>\n\n");
}

/*******************************************************************/

void ShowPromises(struct Bundle *bundles,struct Body *bodies)

{ struct Bundle *bp;
  struct Rlist *rp;
  struct SubType *sp;
  struct Promise *pp;
  struct Body *bdp;
  char *v,rettype,vbuff[CF_BUFSIZE];
  void *retval;

if (GetVariable("control_common","version",&retval,&rettype) != cf_notype)
   {
   v = (char *)retval;
   }
else
   {
   v = "not specified";
   }

ReportBanner("Promises");

snprintf(vbuff,CF_BUFSIZE-1,"Cfengine Site Policy Summary (version %s)",v);

CfHtmlHeader(FREPORT_HTML,vbuff,STYLESHEET,WEBDRIVER,BANNER);
    
fprintf(FREPORT_HTML,"<p>");
  
for (bp = bundles; bp != NULL; bp=bp->next)
   {
   BundleNode(FREPORT_HTML,bp->name);

   fprintf(FREPORT_HTML,"%s Bundle %s%s%s %s%s%s\n",
           CFH[cfx_bundle][cfb],
           CFH[cfx_blocktype][cfb],bp->type,CFH[cfx_blocktype][cfe],
           CFH[cfx_blockid][cfb],bp->name,CFH[cfx_blockid][cfe]);
   
   fprintf(FREPORT_HTML," %s ARGS:%s\n\n",CFH[cfx_line][cfb],CFH[cfx_line][cfe]);

   fprintf(FREPORT_TXT,"Bundle %s in the context of %s\n\n",bp->name,bp->type);
   fprintf(FREPORT_TXT,"   ARGS:\n\n");

   for (rp = bp->args; rp != NULL; rp=rp->next)
      {   
      fprintf(FREPORT_HTML,"%s",CFH[cfx_line][cfb]);
      fprintf(FREPORT_HTML,"   scalar arg %s%s%s\n",CFH[cfx_args][cfb],(char *)rp->item,CFH[cfx_args][cfe]);
      fprintf(FREPORT_HTML,"%s",CFH[cfx_line][cfe]);
      
      fprintf(FREPORT_TXT,"   scalar arg %s\n\n",(char *)rp->item);
      }
  
   fprintf(FREPORT_TXT,"   {\n");   
   fprintf(FREPORT_HTML,"%s",CFH[cfx_promise][cfb]);
   
   for (sp = bp->subtypes; sp != NULL; sp = sp->next)
      {
      fprintf(FREPORT_HTML,"%s",CFH[cfx_line][cfb]);
      TypeNode(FREPORT_HTML,sp->name);
      fprintf(FREPORT_HTML,"%s",CFH[cfx_line][cfe]);
      fprintf(FREPORT_TXT,"   TYPE: %s\n\n",sp->name);
      
      for (pp = sp->promiselist; pp != NULL; pp = pp->next)
         {
         ShowPromise(pp,6);
         }
      }
   
   fprintf(FREPORT_TXT,"   }\n");   
   fprintf(FREPORT_TXT,"\n\n");
   fprintf(FREPORT_HTML,"%s\n",CFH[cfx_promise][cfe]);
   fprintf(FREPORT_HTML,"%s\n",CFH[cfx_line][cfe]);
   fprintf(FREPORT_HTML,"%s\n",CFH[cfx_bundle][cfe]);
   }

/* Now summarize the remaining bodies */

fprintf(FREPORT_HTML,"<h1>All Bodies</h1>");
fprintf(FREPORT_TXT,"\n\nAll Bodies\n\n");

for (bdp = bodies; bdp != NULL; bdp=bdp->next)
   {
   fprintf(FREPORT_HTML,"%s%s\n",CFH[cfx_line][cfb],CFH[cfx_block][cfb]);
   fprintf(FREPORT_HTML,"%s\n",CFH[cfx_promise][cfb]);

   BodyNode(FREPORT_HTML,bdp->name,0);
   ShowBody(bdp,3);

   fprintf(FREPORT_TXT,"\n");
   fprintf(FREPORT_HTML,"%s\n",CFH[cfx_promise][cfe]);
   fprintf(FREPORT_HTML,"%s%s \n ",CFH[cfx_block][cfe],CFH[cfx_line][cfe]);
   fprintf(FREPORT_HTML,"</p>");
   }

CfHtmlFooter(FREPORT_HTML,FOOTER);
}

/*******************************************************************/

void ShowPromise(struct Promise *pp, int indent)

{ struct Constraint *cp;
  struct Body *bp;
  struct FnCall *fp;
  struct Rlist *rp;
  char *v,rettype,vbuff[CF_BUFSIZE];
  void *retval;
  time_t lastseen,last;
  double val,av,var;

if (GetVariable("control_common","version",&retval,&rettype) != cf_notype)
   {
   v = (char *)retval;
   }
else
   {
   v = "not specified";
   }

fprintf(FREPORT_HTML,"%s\n",CFH[cfx_line][cfb]);
fprintf(FREPORT_HTML,"%s\n",CFH[cfx_promise][cfb]);
MapPromiseToTopic(FKNOW,pp,v);
PromiseNode(FREPORT_HTML,pp,0);
fprintf(FREPORT_HTML,"Promise type is %s%s%s, ",CFH[cfx_subtype][cfb],pp->agentsubtype,CFH[cfx_subtype][cfe]);
fprintf(FREPORT_HTML,"<a href=\"#class_context\">context</a> is %s%s%s <br><hr>\n\n",CFH[cfx_class][cfb],pp->classes,CFH[cfx_class][cfe]);

if (pp->promisee)
   {
   fprintf(FREPORT_HTML,"Resource object %s\'%s\'%s promises %s (about %s) to",CFH[cfx_object][cfb],pp->promiser,CFH[cfx_object][cfe],CFH[cfx_object][cfb],pp->agentsubtype);
   ShowRval(FREPORT_HTML,pp->promisee,pp->petype);
   fprintf(FREPORT_HTML,"%s\n\n",CFH[cfx_object][cfe]);
   }
else
   {
   fprintf(FREPORT_HTML,"Resource object %s\'%s\'%s make the promise to default promisee 'cf-%s' (about %s)...\n\n",CFH[cfx_object][cfb],pp->promiser,CFH[cfx_object][cfe],pp->bundletype,pp->agentsubtype);
   }

Indent(indent);
if (pp->promisee != NULL)
   {
   fprintf(FREPORT_TXT,"%s promise by \'%s\' -> ",pp->agentsubtype,pp->promiser);
   ShowRval(FREPORT_TXT,pp->promisee,pp->petype);
   fprintf(FREPORT_TXT," if context is %s\n\n",pp->classes);
   }
else
   {
   fprintf(FREPORT_TXT,"%s promise by \'%s\' (implicit) if context is %s\n\n",pp->agentsubtype,pp->promiser,pp->classes);
   }

for (cp = pp->conlist; cp != NULL; cp = cp->next)
   {
   fprintf(FREPORT_HTML,"%s%s%s => ",CFH[cfx_lval][cfb],cp->lval,CFH[cfx_lval][cfe]);
   Indent(indent+3);
   fprintf(FREPORT_TXT,"%10s => ",cp->lval);

   switch (cp->type)
      {
      case CF_SCALAR:
          if (bp = IsBody(BODIES,(char *)cp->rval))
             {
             ShowBody(bp,15);
             }
          else
             {
             fprintf(FREPORT_HTML,"%s",CFH[cfx_rval][cfb]);
             ShowRval(FREPORT_HTML,cp->rval,cp->type); /* literal */
             fprintf(FREPORT_HTML,"%s",CFH[cfx_rval][cfe]);

             ShowRval(FREPORT_TXT,cp->rval,cp->type); /* literal */
             }
          break;

      case CF_LIST:
          
          rp = (struct Rlist *)cp->rval;
          fprintf(FREPORT_HTML,"%s",CFH[cfx_rval][cfb]);
          ShowRlist(FREPORT_HTML,rp);
          fprintf(FREPORT_HTML,"%s",CFH[cfx_rval][cfe]);
          ShowRlist(FREPORT_TXT,rp);
          break;

      case CF_FNCALL:
          fp = (struct FnCall *)cp->rval;

          if (bp = IsBody(BODIES,fp->name))
             {
             ShowBody(bp,15);
             }
          else
             {
             ShowRval(FREPORT_HTML,cp->rval,cp->type); /* literal */
             ShowRval(FREPORT_TXT,cp->rval,cp->type); /* literal */
             }
          break;
      }
   
   if (cp->type != CF_FNCALL)
      {
      Indent(indent);
      fprintf(FREPORT_HTML," , if body <a href=\"#class_context\">context</a> <span class=\"context\">%s</span>\n",cp->classes);
      fprintf(FREPORT_TXT," if body context %s\n",cp->classes);
      }
     
   }

av = 0;
var = 0;
val = 0;
last = 0;


#ifdef HAVE_LIBCFNOVA
lastseen = GetPromiseCompliance(pp,&val,&av,&var,&last);

if (lastseen) /* This only gives something in Nova or higher */
   {
   strncpy(vbuff,cf_ctime(&lastseen),CF_MAXVARSIZE);
   Chop(vbuff);
   
   fprintf(FREPORT_HTML,"<hr><p><div id=\"compliance\">Compliance last checked on %s. At that time the system was ",vbuff);
   if (val = 1.0)
      {
      fprintf(FREPORT_HTML,"COMPLIANT.");
      }
   else if (val = 0.5)
      {
      fprintf(FREPORT_HTML,"REPAIRED.");
      }
   else if (val = 0.0)
      {
      fprintf(FREPORT_HTML,"NON-COMPLIANT.");
      }

   fprintf(FREPORT_HTML," Average compliance %.1lf pm %.1lf percent. </div>",av*100.0,sqrt(var)*100.0);
   }
#else
fprintf(FREPORT_HTML,"<hr><p><div id=\"compliance\">Compliance level checking only in Cfengine Nova and above</div>",vbuff);
#endif

if (pp->audit)
   {
   Indent(indent);
   fprintf(FREPORT_HTML,"<p><small>Promise (version %s) belongs to bundle <b>%s</b> (type %s) in \'<i>%s</i>\' near line %d</small></p>\n",v,pp->bundle,pp->bundletype,pp->audit->filename,pp->lineno);
   }

fprintf(FREPORT_HTML,"%s\n",CFH[cfx_promise][cfe]);
fprintf(FREPORT_HTML,"%s\n",CFH[cfx_line][cfe]);

if (pp->audit)
   {
   Indent(indent);
   fprintf(FREPORT_TXT,"Promise (version %s) belongs to bundle \'%s\' (type %s) in file \'%s\' near line %d\n",v,pp->bundle,pp->bundletype,pp->audit->filename,pp->lineno);
   fprintf(FREPORT_TXT,"\n\n");
   }
else
   {
   Indent(indent);
   fprintf(FREPORT_TXT,"Promise (version %s) belongs to bundle \'%s\' (type %s) near line %d\n\n",v,pp->bundle,pp->bundletype,pp->lineno);
   }
}

/*******************************************************************/

void ShowScopedVariables()

{ struct Scope *ptr;

fprintf(FREPORT_HTML,"<div id=\"showvars\">");

for (ptr = VSCOPE; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->scope,"this") == 0)
      {
      continue;
      }

   fprintf(FREPORT_HTML,"<p>\nScope %s:\n<br><p>",ptr->scope);
   fprintf(FREPORT_TXT,"\nScope %s:\n",ptr->scope);
   
   if (ptr->hashtable)
      {
      PrintHashes(FREPORT_HTML,ptr->hashtable,1);
      PrintHashes(FREPORT_TXT,ptr->hashtable,0);
      }
   }

fprintf(FREPORT_HTML,"</div>");
}

/*******************************************************************/

void Banner(char *s)

{
CfOut(cf_verbose,"","***********************************************************\n");
CfOut(cf_verbose,""," %s ",s);
CfOut(cf_verbose,"","***********************************************************\n");
}
    
/*******************************************************************/

void ReportBanner(char *s)

{
fprintf(FREPORT_TXT,"***********************************************************\n");
fprintf(FREPORT_TXT," %s \n",s);
fprintf(FREPORT_TXT,"***********************************************************\n");
}
    
/**************************************************************/

void BannerSubType(char *bundlename,char *type,int pass)

{
CfOut(cf_verbose,"","\n");
CfOut(cf_verbose,"","   =========================================================\n");
CfOut(cf_verbose,"","   %s in bundle %s (%d)\n",type,bundlename,pass);
CfOut(cf_verbose,"","   =========================================================\n");
CfOut(cf_verbose,"","\n");
}

/**************************************************************/

void BannerSubSubType(char *bundlename,char *type)

{
if (strcmp(type,"processes") == 0)
   {
   struct Item *ip;
   /* Just parsed all local classes */

   CfOut(cf_verbose,"","     ??? Local class context: \n");

   for (ip = VADDCLASSES; ip != NULL; ip=ip->next)
      {
      printf("       %sǹ",ip->name);
      }

   CfOut(cf_verbose,"","\n");
   }

CfOut(cf_verbose,"","\n");
CfOut(cf_verbose,"","      = = = = = = = = = = = = = = = = = = = = = = = = = = = = \n");
CfOut(cf_verbose,"","      %s in bundle %s\n",type,bundlename);
CfOut(cf_verbose,"","      = = = = = = = = = = = = = = = = = = = = = = = = = = = = \n");
CfOut(cf_verbose,"","\n");
}

/*******************************************************************/

void DebugBanner(char *s)

{
Debug("----------------------------------------------------------------\n");
Debug("  %s                                                            \n",s);
Debug("----------------------------------------------------------------\n");
}

/*******************************************************************/

void Indent(int i)

{ int j;

for (j = 0; j < i; j++)
   {
   fputc(' ',FREPORT_TXT);
   }
}

/*******************************************************************/

void ShowBody(struct Body *body,int indent)

{ struct Rlist *rp;
  struct Constraint *cp;

fprintf(FREPORT_TXT,"%s body for type %s",body->name,body->type);
fprintf(FREPORT_HTML," %s%s%s ",CFH[cfx_blocktype][cfb],body->type,CFH[cfx_blocktype][cfe]);

BodyNode(FREPORT_HTML,body->name,1);

fprintf(FREPORT_HTML,"%s%s%s",CFH[cfx_blockid][cfb],body->name,CFH[cfx_blockid][cfe]);

if (body->args == NULL)
   {
   fprintf(FREPORT_HTML,"%s(no parameters)%s\n",CFH[cfx_args][cfb],CFH[cfx_args][cfe]);
   fprintf(FREPORT_TXT,"(no parameters)\n");
   }
else
   {
   fprintf(FREPORT_HTML,"(");
   fprintf(FREPORT_TXT,"\n");
   
   for (rp = body->args; rp != NULL; rp=rp->next)
      {
      if (rp->type != CF_SCALAR)
         {
         FatalError("ShowBody - non-scalar paramater container");
         }

      fprintf(FREPORT_HTML,"%s%s%s,\n",CFH[cfx_args][cfb],(char *)rp->item,CFH[cfx_args][cfe]);
      Indent(indent);
      fprintf(FREPORT_TXT,"arg %s\n",(char *)rp->item);
      }

   fprintf(FREPORT_HTML,")");
   fprintf(FREPORT_TXT,"\n");
   }

BodyNode(FREPORT_HTML,body->name,2);

Indent(indent);
fprintf(FREPORT_TXT,"{\n");

for (cp = body->conlist; cp != NULL; cp=cp->next)
   {
   fprintf(FREPORT_HTML,"%s.....%s%s => ",CFH[cfx_lval][cfb],cp->lval,CFH[cfx_lval][cfe]);
   Indent(indent);
   fprintf(FREPORT_TXT,"%s => ",cp->lval);

   fprintf(FREPORT_HTML,"\'%s",CFH[cfx_rval][cfb]);

   ShowRval(FREPORT_HTML,cp->rval,cp->type); /* literal */
   ShowRval(FREPORT_TXT,cp->rval,cp->type); /* literal */

   fprintf(FREPORT_HTML,"\'%s",CFH[cfx_rval][cfe]);

   if (cp->classes != NULL)
      {
      fprintf(FREPORT_HTML," if sub-body context %s%s%s\n",CFH[cfx_class][cfb],cp->classes,CFH[cfx_class][cfe]);
      fprintf(FREPORT_TXT," if sub-body context %s\n",cp->classes);
      }
   else
      {
      fprintf(FREPORT_TXT,"\n");
      }
   }

Indent(indent);
fprintf(FREPORT_TXT,"}\n");
}

/*******************************************************************/

void SyntaxTree(void)

{
printf("%s",CFH[0][0]);

printf("<table class=frame><tr><td>\n");
printf("<h1>CFENGINE %s SYNTAX</h1><p>",VERSION);

ShowDataTypes();
ShowControlBodies();
ShowBundleTypes();
ShowBuiltinFunctions();
printf("</td></tr></table>\n");
printf("%s",CFH[0][1]);
}

/*******************************************************************/
/* Level 2                                                         */
/*******************************************************************/

void ShowDataTypes()

{ int i;

printf("<table class=border><tr><td><h1>Promise datatype legend</h1>\n");
printf("<ol>\n");

for (i = 0; strcmp(CF_DATATYPES[i],"<notype>") != 0; i++)
   {
   printf("<li>%s</li>\n",CF_DATATYPES[i]);
   }

printf("</ol></td></tr></table>\n\n");
}

/*******************************************************************/

void ShowBundleTypes()

{ int i;

printf("<h1>Bundle types (software components)</h1>\n");

printf("<div id=\"bundles\">");
printf("<ul>\n");

for (i = 0; CF_ALL_BODIES[i].btype != NULL; i++)
   {
   printf("<li>COMPONENT %s</li>\n", CF_ALL_BODIES[i].btype);
   ShowPromiseTypesFor(CF_ALL_BODIES[i].btype);
   }

printf("</ul></div>\n\n");
}


/*******************************************************************/

void ShowPromiseTypesFor(char *s)

{ int i,j;
  struct SubTypeSyntax *st;

printf("<div id=\"promisetype\">");
printf("<h4>Promise types for %s bundles</h4>\n",s);
printf("<ul>\n");
printf("<table class=border><tr><td>\n");

for (i = 0; i < CF3_MODULES; i++)
   {
   st = CF_ALL_SUBTYPES[i];

   for (j = 0; st[j].btype != NULL; j++)
      {
      if (strcmp(s,st[j].btype) == 0 || strcmp("*",st[j].btype) == 0)
         {
         printf("<li><h4>PROMISE TYPE %s</h4>\n",st[j].subtype);
         ShowBodyParts(st[j].bs);
         printf("</li>\n");
         }
      }
   }

printf("</td></tr></table>\n");
printf("</ul></div>\n\n");
}

/*******************************************************************/

void ShowBodyParts(struct BodySyntax *bs)

{ int i;

if (bs == NULL)
   {
   return;
   }
 
printf("<div id=bodies><table class=border>\n");

for (i = 0; bs[i].lval != NULL; i++)
   {
   if (bs[i].range == (void *)CF_BUNDLE)
      {
      printf("<tr><td>%s</td><td>%s</td><td>(Separate Bundle)</td></tr>\n",bs[i].lval,CF_DATATYPES[bs[i].dtype]);
      }
   else if (bs[i].dtype == cf_body)
      {
      printf("<tr><td>%s</td><td>%s</td><td>",bs[i].lval,CF_DATATYPES[bs[i].dtype]);
      ShowBodyParts((struct BodySyntax *)bs[i].range);
      printf("</td></tr>\n");
      }
   else
      {
      printf("<tr><td>%s</td><td>%s</td><td>",bs[i].lval,CF_DATATYPES[bs[i].dtype]);
      ShowRange((char *)bs[i].range,bs[i].dtype);
      printf("</td><td>");
      printf("<div id=\"description\">%s</div>",bs[i].description);
      printf("</td></tr>\n");
      }
   }

printf("</table></div>\n");
}

/*******************************************************************/

void ShowRange(char *s,enum cfdatatype type)

{ char *sp;
 
if (strlen(s) == 0)
   {
   printf("(arbitrary string)");
   return;
   }

switch (type)
   {
   case cf_opts:
   case cf_olist:
       
       for (sp = s; *sp != '\0'; sp++)
          {
          printf("%c",*sp);
          if (*sp == ',')
             {
             printf("<br>");
             }
          }

       break;

   default:
       for (sp = s; *sp != '\0'; sp++)
          {
          printf("%c",*sp);
          if (*sp == '|')
             {
             printf("<br>");
             }
          }
   }
}

/*******************************************************************/

void ShowBuiltinFunctions()

{ int i;

printf("<h1>builtin functions</h1>\n");
 
printf("<center><table id=functionshow>\n");
printf("<tr><th>Return type</th><th>Function name</th><th>Arguments</th><th>Description</th></tr>\n");

for (i = 0; CF_FNCALL_TYPES[i].name != NULL; i++)
   {
   printf("<tr><td>%s</td><td>%s()</td><td>%d args expected</td><td>%s</td></tr>\n",
          CF_DATATYPES[CF_FNCALL_TYPES[i].dtype],
          CF_FNCALL_TYPES[i].name,
          CF_FNCALL_TYPES[i].numargs,
          CF_FNCALL_TYPES[i].description
          );
   }

printf("</table></center>\n");
}

/*******************************************************************/

void ReportError(char *s)

{
if (PARSING)
   {
   yyerror(s);
   }
else
   {
   Chop(s);
   fprintf(stderr,"Validation: %s\n",s);
   }
}
