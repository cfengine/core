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
/* File: report.c                                                            */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

enum cfx_formatindex
   {
   cfb,
   cfe,
   };

enum cfx_format
   {
   cfx_head,
   cfx_bundle,
   cfx_block,
   cfx_blockheader,
   cfx_blockid,
   cfx_blocktype,
   cfx_args,
   cfx_promise,
   cfx_class,
   cfx_subtype,
   cfx_object,
   cfx_lval,
   cfx_rval,
   cfx_qstring,
   cfx_rlist,
   cfx_function,
   cfx_line,
   };


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
    "<html><head>\n<link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/css/promises.css\" />\n</head>\n","</html>",
    "<table class=border><tr><td><h2>","</td></tr></h2></table>",
    "<p><table class=border cellpadding=5 width=800>","</table>",
    "<tr><th>","</th></tr>",
    "<font color=red>","</font>",
    "<font color=blue>","</font>",
    "<b>","</b>",
    "<tr><td bgcolor=#fefdec></td><td bgcolor=#fefeec><table class=border width=800><tr><td bgcolor=#ffffff>","</td></tr></table></td></tr>",
    "<i><font color=blue>","</font></i>",
    "<b><font color=green size=4>","</font><b>",
    "<b>","</b>",
    "<br><font color=green>........................","</font>",
    "","",
    "","",
    "<i>","</i>",
    "","",
    "<tr><td colspan=\"2\">","</td></tr>",
    NULL,NULL
   };

/*******************************************************************/
/* Generic                                                         */
/*******************************************************************/

void ShowContext(void)

{ struct Item *ptr;

if (!XML)
   {
   ReportBanner("Agent's basic classified context");

   CfOut(cf_verbose,"","Defined Classes = ( ");
   
   for (ptr = VHEAP; ptr != NULL; ptr=ptr->next)
      {
      if (VERBOSE||DEBUG)
         {
         printf("%s ",ptr->name);
         }
      }
   
      if (VERBOSE||DEBUG)
         {
         printf(")\n");
         }
   
   CfOut (cf_verbose,"","\nNegated Classes = ( ");
   
   for (ptr = VNEGHEAP; ptr != NULL; ptr=ptr->next)
      {
      if (VERBOSE||DEBUG)
         {
         printf("%s ",ptr->name);
         }
      }

   if (VERBOSE||DEBUG)
      {      
      printf (")\n");
      }
   }
else
   {
   fprintf(FREPORT,"<h1>Agent's basic classified context</h1>\n");

   fprintf(FREPORT,"<table><tr>");
   fprintf(FREPORT,"<td>Defined Classes</td><td><ul>");
   
   for (ptr = VHEAP; ptr != NULL; ptr=ptr->next)
      {
      fprintf(FREPORT,"<li>%s%s%s \n",CFH[cfx_class][cfb],ptr->name,CFH[cfx_class][cfe]);
      }
   
   fprintf(FREPORT,"</ul></td></tr><tr>\n");
   
   fprintf(FREPORT,"<td>Negated Classes</td><td><ul>");
   
   for (ptr = VNEGHEAP; ptr != NULL; ptr=ptr->next)
      {
      fprintf(FREPORT,"<li>%s%s%s \n",CFH[cfx_class][cfb],ptr->name,CFH[cfx_class][cfe]);
      }
   
   fprintf(FREPORT,"</ul></td></tr></table><p>\n");
   }
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

if (!XML)
   {
   ReportBanner("Promises");
   }
else
   {
   fprintf(FREPORT,"<p>");
   fprintf(FREPORT,"<h1>Promise Bundles</h1> ");
   fprintf(FREPORT,"%s\n",CFH[cfx_head][cfb]);
   }
  
for (bp = bundles; bp != NULL; bp=bp->next)
   {
   if (XML)
      {
      fprintf(FREPORT,"<a name=\"Bundle_%s\"></a>\n%s\n",CanonifyName(bp->name),CFH[cfx_block][cfb]);
      fprintf(FREPORT,"%s\n",CFH[cfx_line][cfb]);
      fprintf(FREPORT,"%s Bundle %s%s%s %s%s%s %s\n",
             CFH[cfx_bundle][cfb],
             CFH[cfx_blocktype][cfb],bp->type,CFH[cfx_blocktype][cfe],
             CFH[cfx_blockid][cfb],bp->name,CFH[cfx_blockid][cfe],
             CFH[cfx_bundle][cfe]);
      fprintf(FREPORT,"%s\n",CFH[cfx_line][cfe]);
      }
   else
      {
      fprintf(FREPORT,"Bundle %s in the context of %s\n\n",bp->name,bp->type);
      }

   if (XML)
      {
      fprintf(FREPORT," %s ARGS:%s %s\n\n",CFH[cfx_line][cfb],CFH[cfx_line][cfe],CFH[cfx_promise][cfb]);
      }
   else
      {
      fprintf(FREPORT,"   ARGS:\n\n");
      }
   
   for (rp = bp->args; rp != NULL; rp=rp->next)
      {
      if (XML)
         {
         fprintf(FREPORT,"%s",CFH[cfx_line][cfb]);
         fprintf(FREPORT,"   scalar arg %s%s%s\n",CFH[cfx_args][cfb],(char *)rp->item,CFH[cfx_args][cfe]);
         fprintf(FREPORT,"%s",CFH[cfx_line][cfe]);
         }
      else
         {
         fprintf(FREPORT,"   scalar arg %s\n\n",(char *)rp->item);
         }
      }

   if (XML)
      {
      fprintf(FREPORT,"%s\n%s\n",CFH[cfx_promise][cfe],CFH[cfx_line][cfb]);
      }
   else
      {
      fprintf(FREPORT,"   {\n");
      }

   for (sp = bp->subtypes; sp != NULL; sp = sp->next)
      {
      if (XML)
         {
         fprintf(FREPORT,"%s",CFH[cfx_line][cfb]);
         fprintf(FREPORT,"<a name=\"Type_%s\"></a>\n",CanonifyName(sp->name));
         fprintf(FREPORT,"   TYPE: %s%s%s\n\n",CFH[cfx_subtype][cfb],sp->name,CFH[cfx_subtype][cfe]);
         fprintf(FREPORT,"%s",CFH[cfx_line][cfe]);
         }
      else
         {
         fprintf(FREPORT,"   TYPE: %s\n\n",sp->name);
         }
      
      for (pp = sp->promiselist; pp != NULL; pp = pp->next)
         {
         ShowPromise(pp,6);
         }
      }

   if (XML)
      {
      fprintf(FREPORT,"%s \n ",CFH[cfx_block][cfe]);
      fprintf(FREPORT,"%s\n",CFH[cfx_head][cfe]);
      }
   else
      {
      fprintf(FREPORT,"   }\n");   
      fprintf(FREPORT,"\n\n");
      }
   }

/* Now summarize the remaining bodies */

if (XML)
   {
   fprintf(FREPORT,"<h1>All Bodies</h1>");
   }
else
   {
   fprintf(FREPORT,"\n\nAll Bodies\n\n");
   }

for (bdp = bodies; bdp != NULL; bdp=bdp->next)
   {
   if (XML)
      {
      fprintf(FREPORT,"%s\n",CFH[cfx_block][cfb]);
      fprintf(FREPORT,"%s\n",CFH[cfx_promise][cfb]);
      }

   ShowBody(bdp,3);

   if (!XML)
      {
      fprintf(FREPORT,"\n");
      }
   else
      {
      fprintf(FREPORT,"%s\n",CFH[cfx_promise][cfe]);
      fprintf(FREPORT,"%s \n ",CFH[cfx_block][cfe]);
      fprintf(FREPORT,"</p>");
      }
   }
}

/*******************************************************************/

void ShowPromise(struct Promise *pp, int indent)

{ struct Constraint *cp;
  struct Body *bp;
  struct FnCall *fp;
  struct Rlist *rp;
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

MapPromiseToTopic(FKNOW,pp,v);

if (XML)
   {
   fprintf(FREPORT,"%s\n",CFH[cfx_line][cfb]);
   fprintf(FREPORT,"%s\n",CFH[cfx_promise][cfb]);
   fprintf(FREPORT,"Promise type is %s%s%s, ",CFH[cfx_class][cfb],pp->agentsubtype,CFH[cfx_class][cfe]);
   fprintf(FREPORT,"context is %s%s%s <br><hr>\n\n",CFH[cfx_class][cfb],pp->classes,CFH[cfx_class][cfe]);

   snprintf(vbuff,CF_BUFSIZE,"%s_%s",pp->promiser,pp->audit->filename);
   fprintf(FREPORT,"<a name=\"Promise_%s_%d\"></a>\n",CanonifyName(vbuff),pp->lineno);

   if (pp->promisee)
      {
      fprintf(FREPORT,"Resource object %s\'%s\'%s promises %s (about %s) to",CFH[cfx_object][cfb],pp->promiser,CFH[cfx_object][cfe],CFH[cfx_object][cfb],pp->agentsubtype);
      ShowRval(FREPORT,pp->promisee,pp->petype);
      fprintf(FREPORT,"%s\n\n",CFH[cfx_object][cfe]);
      }
   else
      {
      fprintf(FREPORT,"Resource object %s\'%s\'%s make the promise to default promisee 'cf-%s' (about %s)...\n\n",CFH[cfx_object][cfb],pp->promiser,CFH[cfx_object][cfe],pp->bundletype,pp->agentsubtype);
      }
   }
else
   {   
   Indent(indent);
   if (pp->promisee != NULL)
      {
      fprintf(FREPORT,"%s promise by \'%s\' -> ",pp->agentsubtype,pp->promiser);
      ShowRval(FREPORT,pp->promisee,pp->petype);
      fprintf(FREPORT," if context is %s\n\n",pp->classes);
      }
   else
      {
      fprintf(FREPORT,"%s promise by \'%s\' (implicit) if context is %s\n\n",pp->agentsubtype,pp->promiser,pp->classes);
      }
   }
  
for (cp = pp->conlist; cp != NULL; cp = cp->next)
   {
   if (XML)
      {
      fprintf(FREPORT,"%s%s%s => ",CFH[cfx_lval][cfb],cp->lval,CFH[cfx_lval][cfe]);
      }
   else
      {
      Indent(indent+3);
      fprintf(FREPORT,"%10s => ",cp->lval);
      }

   fprintf(FREPORT,"%s",CFH[cfx_rval][cfb]);

   switch (cp->type)
      {
      case CF_SCALAR:
          if (bp = IsBody(BODIES,(char *)cp->rval))
             {
             ShowBody(bp,15);
             }
          else
             {
             ShowRval(FREPORT,cp->rval,cp->type); /* literal */
             }
          break;

      case CF_LIST:
          
          rp = (struct Rlist *)cp->rval;
          ShowRlist(FREPORT,rp);
          break;

      case CF_FNCALL:
          fp = (struct FnCall *)cp->rval;

          if (bp = IsBody(BODIES,fp->name))
             {
             ShowBody(bp,15);
             }
          else
             {
             ShowRval(FREPORT,cp->rval,cp->type); /* literal */
             }
          break;
      }

   fprintf(FREPORT,"%s",CFH[cfx_rval][cfe]);
   
   if (XML)
      {
      if (cp->type != CF_FNCALL)
         {
         fprintf(FREPORT," , if body context %s\n",cp->classes);
         }
      }
   else
      {
      if (cp->type != CF_FNCALL)
         {
         fprintf(FREPORT," if body context %s\n",cp->classes);
         }
      }
   }

if (XML)
   {
   fprintf(FREPORT,"<p><small>Promise (version %s) belongs to bundle <b>%s</b> (type %s) in \'<i>%s</i>\' near line %d</small></p>\n",v,pp->bundle,pp->bundletype,pp->audit->filename,pp->lineno);
   fprintf(FREPORT,"%s\n",CFH[cfx_promise][cfe]);
   fprintf(FREPORT,"%s\n",CFH[cfx_line][cfe]);
   }
else
   {
   if (pp->audit)
      {
      fprintf(FREPORT,"Promise (version %s) belongs to bundle \'%s\' (type %s) in file \'%s\' near line %d\n",v,pp->bundle,pp->bundletype,pp->audit->filename,pp->lineno);
      fprintf(FREPORT,"\n");
      }
   else
      {
      fprintf(FREPORT,"Promise (version %s) belongs to bundle \'%s\' (type %s) near line %d\n",v,pp->bundle,pp->bundletype,pp->lineno);
      }
   }
}

/*******************************************************************/

void ShowScopedVariables(FILE *fp)

{ struct Scope *ptr;

for (ptr = VSCOPE; ptr != NULL; ptr=ptr->next)
   {
   if (strcmp(ptr->scope,"this") == 0)
      {
//      continue;
      }
   
   if (XML)
      {
      fprintf(fp,"<p>\nConstant variables in SCOPE %s:\n<br><p>",ptr->scope);
      }
   else
      {
      fprintf(fp,"\nConstant variables in SCOPE %s:\n",ptr->scope);
      }
   
   if (ptr->hashtable)
      {
      PrintHashes(fp,ptr->hashtable);
      }
   }
}

/*******************************************************************/

void Banner(char *s)

{
CfOut(cf_verbose,"","***********************************************************\n");
CfOut(cf_verbose,""," %s \n",s);
CfOut(cf_verbose,"","***********************************************************\n");
}
    
/*******************************************************************/

void ReportBanner(char *s)

{
fprintf(FREPORT,"***********************************************************\n");
fprintf(FREPORT," %s \n",s);
fprintf(FREPORT,"***********************************************************\n");
}
    
/**************************************************************/

void BannerSubType(char *bundlename,char *type)

{
Verbose("\n");
Verbose("   =========================================================\n");
Verbose("   %s in bundle %s\n",type,bundlename);
Verbose("   =========================================================\n");
Verbose("\n");
}

/**************************************************************/

void BannerSubSubType(char *bundlename,char *type)

{
if (strcmp(type,"processes") == 0)
   {
   struct Item *ip;
   /* Just parsed all local classes */

   Verbose("     ??? Local class context: \n");

   for (ip = VADDCLASSES; ip != NULL; ip=ip->next)
      {
      printf("       %sǹ",ip->name);
      }

   Verbose("\n");
   }

Verbose("\n");
Verbose("      = = = = = = = = = = = = = = = = = = = = = = = = = = = = \n");
Verbose("      %s in bundle %s\n",type,bundlename);
Verbose("      = = = = = = = = = = = = = = = = = = = = = = = = = = = = \n");
Verbose("\n");
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
   fputc(' ',FREPORT);
   }
}


/*******************************************************************/

void ShowBody(struct Body *body,int indent)

{ struct Rlist *rp;
  struct Constraint *cp;

if (!XML)
   {
   fprintf(FREPORT,"%s body for type %s",body->name,body->type);
   }
else
   {
   fprintf(FREPORT," %s%s%s %s%s%s",CFH[cfx_blocktype][cfb],body->type,CFH[cfx_blocktype][cfe],
          CFH[cfx_blockid][cfb],body->name,CFH[cfx_blockid][cfe]);
   }

if (body->args == NULL)
   {
   if (XML)
      {
      fprintf(FREPORT,"%s(no parameters)%s\n",CFH[cfx_args][cfb],CFH[cfx_args][cfe]);
      }
   else
      {
      fprintf(FREPORT,"(no parameters)\n");
      }
   }
else
   {
   if (XML)
      {
      fprintf(FREPORT,"(");
      }
   else
      {
      fprintf(FREPORT,"\n");
      }
   
   for (rp = body->args; rp != NULL; rp=rp->next)
      {
      if (rp->type != CF_SCALAR)
         {
         FatalError("ShowBody - non-scalar paramater container");
         }

      if (XML)
         {
         fprintf(FREPORT,"%s%s%s,\n",CFH[cfx_args][cfb],(char *)rp->item,CFH[cfx_args][cfe]);
         }
      else
         {
         Indent(indent);
         fprintf(FREPORT,"arg %s\n",(char *)rp->item);
         }
      }

   if (XML)
      {
      fprintf(FREPORT,")");
      }
   else
      {
      fprintf(FREPORT,"\n");
      }
   }

if (XML)
   {
   }
else
   {   
   Indent(indent);
   fprintf(FREPORT,"{\n");
   }

for (cp = body->conlist; cp != NULL; cp=cp->next)
   {
   if (XML)
      {
      fprintf(FREPORT,"%s.....%s%s => ",CFH[cfx_lval][cfb],cp->lval,CFH[cfx_lval][cfe]);
      }
   else{
      Indent(indent);
      fprintf(FREPORT,"%s => ",cp->lval);
      }
   
   ShowRval(FREPORT,cp->rval,cp->type); /* literal */

   if (cp->classes != NULL)
      {
      if (XML)
         {
         fprintf(FREPORT," if subbody context %s%s%s\n",CFH[cfx_class][cfb],cp->classes,CFH[cfx_class][cfe]);
         }
      else
         {
         fprintf(FREPORT," if subbody context %s\n",cp->classes);
         }
      }
   else
      {
      if (XML)
         {
         }
      else
         {
         fprintf(FREPORT,"\n");
         }
      }
   }

if (XML)
   {
   }
else
   {
   Indent(indent);
   fprintf(FREPORT,"}\n");
   }
}



/*******************************************************************/

void SyntaxTree()

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
      ShowRange((char *)bs[i].range);
      printf("</td><td>");
      printf("<div id=\"description\">%s</div>",bs[i].description);
      printf("</td></tr>\n");
      }
   }

printf("</table></div>\n");
}

/*******************************************************************/

void ShowRange(char *s)

{ char *sp;
 
if (strlen(s) == 0)
   {
   printf("(arbitrary string)");
   return;
   }

for (sp = s; *sp != '\0'; sp++)
   {
   printf("%c",*sp);
   if (*sp == '|')
      {
      printf("<br>");
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
