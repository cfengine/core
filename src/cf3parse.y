
%{

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

#include "cf3.defs.h"
#include "parser_state.h"

#include "constraints.h"
#include "env_context.h"
// FIX: remove
#include "syntax.h"

#define HvBDebug printf
/*
#define HvBDebug 
*/

extern char *yytext;
extern int *yyleng;
extern int yylineno;

static void DebugBanner(const char *s);
static void fatal_yyerror(const char *s);

/* HvB additions */
#include "mod_files.h"
static void yyerror_hvb(const char *s);
static bool BodyTypeSyntaxLookup(const char *, const char *, char *);
static BodySyntax *extra_bodysyntax_p = NULL;
static char error_txt[CF_MAXVARSIZE];

/* end HvB */ 

static bool INSTALL_SKIP = false;

#define YYMALLOC xmalloc

/*
 * HvB lexer vars
*/
extern char cf_linebuf[8192];
extern int  cf_tokenpos;
extern int  cf_lineno;
extern int  cf_block_open;



%}

/*
%union{
    char    text[CF_MAXVARSIZE];
    int     value;
}
*/

%token BLOCKID QSTRING CLASS CATEGORY BUNDLE BODY ASSIGN ARROW NAKEDVAR

/*
 HVB
*/
%token COMMON AGENT EDITLINE  EDITXML
%token BLOCK_OPEN BLOCK_CLOSE 
%token VARS_CATEGORY CLASSES_CATEGORY INTERFACES_CATEGORY 
%token PROCESSES_CATEGORY STORAGE_CATEGORY PACKAGES_CATEGORY 
%token COMMANDS_CATEGORY METHODS_CATEGORY FILES_CATEGORY
%token DATABASES_CATEGORY SERVICES_CATEGORY REPORTS_CATEGORY
%token EDITLINE_FIELD_EDITS_CATEGORY EDITLINE_INSERT_LINES_CATEGORY EDITLINE_REPLACE_PATTERNS_CATEGORY 
%token EDITLINE_DELETE_LINES_CATEGORY EDITLINE_CATEGORY EDITXML_CATEGORY UNKNOWN_CATEGORY

%token  BLOCK_IDSYNTAX
%token  IDSYNTAX


%%

specification:       /* empty */
                     | blocks

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

blocks:                block
                     | blocks block;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

block:                 bundles
                     | bodies

bundles:              bundle
                    | bundles bundle

bodies:               body
                    | bodies body
                    
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bundle:                bundle_syntax

bundle_syntax:         BUNDLE bundle_type bundle_id bundle_body
                     | BUNDLE bundle_type bundle_id usearglist bundle_body
                       {
                         printf("Parsed a bundle\n");
                       };
                       | error
                       {
                          yyerror_hvb("bundle type is not valid\n");
                          exit(1);
                       }

bundle_type:        bundle_values
                       {
                          P.block = "bundle";
                          strncpy(P.blocktype, yytext, CF_MAXVARSIZE);

                          DebugBanner("Bundle");
                          HvBDebug("P:bundle:%s\n", P.blocktype);

                          P.rval = (Rval) { NULL, '\0' };
                          DeleteRlist(P.currentRlist);
                          P.currentRlist = NULL;
                          P.currentstring = NULL;
                          strcpy(P.blockid,"");

                       }

bundle_values:         COMMON
                     | AGENT
                     | EDITLINE
                     | EDITXML
                     | error
                       {
                          yyerror_hvb("Unknown bundle type");
                       }

bundle_id:           bundle_id_syntax
                     {
                         strncpy(P.blockid,yytext,CF_MAXVARSIZE);
                         HvBDebug("\tP:bundle:%s:%s\n", P.blocktype, P.blockid);
                     }

bundle_id_syntax:    typeid
                   | blockid
 

bundle_body:         BLOCK_OPEN 
                     {

                        HvBDebug("\tP:Bundle Block open\n");

                        /*
                         * This irrelevant because we know we have parsed the right cfengie
                         * bundles
               
                        if (RelevantBundle(CF_AGENTTYPES[THIS_AGENT_TYPE], P.blocktype))
                        {
                            HvBDebug("We a compiling everything here\n");
                            INSTALL_SKIP = false;
                        }
                        else if (strcmp(CF_AGENTTYPES[THIS_AGENT_TYPE], P.blocktype) != 0)
                        {
                               HvBDebug("This is for a different agent\n");
                               INSTALL_SKP = true;
                        }

                        */

                        INSTALL_SKIP = false; 
                        if (!INSTALL_SKIP) 
                        {
                            P.currentbundle = AppendBundle(P.policy, P.blockid, P.blocktype, P.useargs, P.filename);
                            P.currentbundle->offset.line = P.line_no;
                            P.currentbundle->offset.start = P.offsets.last_block_id;
                        }
                        else
                        {
                           P.currentbundle = NULL;
                        }
                        
                        DeleteRlist(P.useargs);
                        P.useargs = NULL;

                     }

                     bundle_statements

                     BLOCK_CLOSE
                     {
                        HvBDebug("P:Bundle Block close\n");
                        INSTALL_SKIP = false;
                        P.offsets.last_id = -1;
                        P.offsets.last_string = -1;
                        P.offsets.last_class_id = -1;

                        if (P.currentbundle)
                        {
                            P.currentbundle->offset.end = P.offsets.current;
                        }
                     }

bundle_statements:    bundle_statement
                    | bundle_statements bundle_statement 

bundle_statement:   categories
                    {
                        HvBDebug("\tP: Ending bundle categories \n");
                    }

categories:         category
                  | categories category

category:           category_type
                    {
                       /* 
                        reset check for category
                       */

                       SubTypeSyntax ss;
                       size_t        token_size = strlen(yytext);

                       P.line_pos += token_size;
                       P.offsets.last_subtype_id = P.offsets.current - token_size;
                       yytext[token_size - 1] = '\0'; 
                       HvBDebug("\tP:%s:%s:%s category_syntax = %s\n", P.block, P.blocktype, P.blockid, yytext); 
                       strncpy(P.currenttype, yytext, CF_MAXVARSIZE); 

                       /*
                        * Is the valid category for agent
                       */
                       ss = SubTypeSyntaxLookup(P.blocktype, P.currenttype);
                       if ( ss.bundle_type == NULL )
                       {
                          sprintf(error_txt, "Category: '%s' is not a valid type for bundle type: '%s", 
                                    P.currenttype, P.blocktype);
                          yyerror_hvb(error_txt);
                       }
                       
                       if (!INSTALL_SKIP)
                       {
                          HvBDebug("\tP: inside bundle\n");
                          P.currentstype = AppendSubType(P.currentbundle,P.currenttype); 
                          P.currentstype->offset.line = P.line_no;
                          P.currentstype->offset.start = P.offsets.last_subtype_id;
                       }
                       else
                       {
                           P.currentstype = NULL;
                       } 
                       
                       if (P.currentclasses != NULL)
                       {
                          free(P.currentclasses);
                          P.currentclasses = NULL;
                       }
                     }
                     category_statements



category_statements:   class_or_promise
                     | category_statements class_or_promise
                     | ;
                         

class_or_promise:        class
                       | promise ';'
                       | error
                         {
                             yyerror_hvb("check previous statement, expected ';'\n");
                         }


promise:                 promiser
                       | promiser constraints
                       | promiser constraint error
                         {
                             yyerror_hvb("check previous statement, expected ',' \n");
                         }


category_type:           REPORTS_CATEGORY 
                            { extra_bodysyntax_p = NULL; }

                       | INTERFACES_CATEGORY          
                            { extra_bodysyntax_p = NULL; }

                       | PROCESSES_CATEGORY  
                            { extra_bodysyntax_p = NULL; } 

                       | PACKAGES_CATEGORY   
                            { extra_bodysyntax_p = NULL; }

                       | DATABASES_CATEGORY            
                            { extra_bodysyntax_p = NULL; }

                       | VARS_CATEGORY     
                            {  extra_bodysyntax_p = NULL; }

                       | COMMANDS_CATEGORY 
                            { extra_bodysyntax_p = NULL; }

                       | CLASSES_CATEGORY  
                            { extra_bodysyntax_p = NULL; }

                       | METHODS_CATEGORY
                            { extra_bodysyntax_p = NULL; }

                       | SERVICES_CATEGORY
                            { extra_bodysyntax_p = NULL; }

                       | FILES_CATEGORY      
                            { extra_bodysyntax_p = NULL; }

                       | EDITLINE_CATEGORY

                            { extra_bodysyntax_p = (BodySyntax *)CF_COMMON_EDITBODIES; }

                       | EDITXML_CATEGORY
                            { extra_bodysyntax_p = (BodySyntax *)CF_COMMON_XMLBODIES; }

                       | UNKNOWN_CATEGORY
                         {
                            sprintf(error_txt,"'%s' is not a valid category for bundle '%s'", yytext, P.blocktype);
                            yyerror_hvb(error_txt);
                         }


constraints:          constraint  
                    | constraints ',' constraint
                    |;

constraint:           promiser_type         
                      { 

                          strncpy(P.lval, yytext, CF_MAXVARSIZE); 
                          HvBDebug("\tP:%s:%s:%s:%s promiser type for LVAL '%s'\n",
                              P.block, P.blocktype, P.blockid, P.currenttype, P.lval);

                          DeleteRlist(P.currentRlist);
                          P.currentRlist = NULL;

                      }
                      ASSIGN
                      {
                        HvBDebug("\tP:ASSIGN\n");
                      }
                      rval_bundle_statement
                    | error
                      {
                        yyerror_hvb("promise line statement error\n");
                      }

promiser_type:       promiser_id 
                     {

                        SubTypeSyntax ss;
                        BodySyntax *valid_types_p;
                        BodySyntax *tmp_p;
                        bool       found = false;


                        ss = SubTypeSyntaxLookup(P.blocktype, P.currenttype);
                        valid_types_p = ss.bs;

                        while ( valid_types_p->lval != NULL )
                        {

                           /*
                             printf("Hvb keyword = %s\n", valid_types_p->lval);
                           */
                           if (strcmp(yytext, valid_types_p->lval) == 0)
                           {
                              found = true; 
                              break; 
                           }
                           else
                           {
                              valid_types_p++;
                           }

                        }


                        /*
                         * some bundle type have a common settings for bs
                         * eg: edit_line, edit_xml
                        */
                        if (!found)
                        {
                           if ( extra_bodysyntax_p != NULL )
                           {
                              BodySyntax *tmp_p;

                              tmp_p = extra_bodysyntax_p;
                              while ( tmp_p->lval != NULL )
                              {
                                 if (strcmp(yytext, tmp_p->lval) == 0)
                                 {
                                    found = true; 
                                    break;
                                 }
                                 else
                                 {
                                    tmp_p++;
                                 }
                             }
                           }
                         }


                         /*
                          * all categories support this type so search again
                         */
                         if (!found)
                         {
                            tmp_p = (BodySyntax *)CF_COMMON_BODIES;
                            while ( tmp_p->lval != NULL )
                            {
                               if (strcmp(yytext, tmp_p->lval) == 0)
                               {
                                  found = true;
                                  break;
                               }
                               else
                               {
                                  tmp_p++;
                               }
                            }
                          }

                          /* 
                           * print an error message
                          */ 
                          if ( !found )
                          {
                             sprintf(error_txt, "'%s' is not allowed as promise type for category: '%s'", 
                                           yytext, P.currenttype);
                             yyerror_hvb(error_txt);
                          }

                       }

promiser_id:          id


rval_bundle_statement:   rval_type
                         {
                                /*
                                HvBDebug("\tP:%s:%s:%s:%s for rval '%s'\n", P.block, P.blocktype, P.blockid, P.currenttype, yytext); 
                                */
                                
                                if (!INSTALL_SKIP)
                                {
                                   HvBDebug("\tAdd Constraint to promiser: %s\n\n", P.promiser);
                                   Constraint *cp = NULL;
                                   SubTypeSyntax ss = SubTypeSyntaxLookup(P.blocktype,P.currenttype);
                                   CheckConstraint(P.currenttype, CurrentNameSpace(P.policy), P.blockid, P.lval, P.rval, ss);
                                   cp = ConstraintAppendToPromise(P.currentpromise, P.lval, P.rval, "any", P.references_body);
                                   cp->offset.line = P.line_no;
                                   cp->offset.start = P.offsets.last_id;
                                   cp->offset.end = P.offsets.current;
                                   cp->offset.context = P.offsets.last_class_id;
                                   P.currentstype->offset.end = P.offsets.current;
                                   
                                   // Cache whether there are subbundles for later $(this.promiser) logic 
                                   
                                   if (strcmp(P.lval,"usebundle") == 0 || strcmp(P.lval,"edit_line") == 0
                                      || strcmp(P.lval,"edit_xml") == 0)
                                   {
                                      P.currentpromise->has_subbundles = true;
                                   }

                                   P.rval = (Rval) { NULL, '\0' };
                                   strcpy(P.lval,"no lval");
                                   DeleteRlist(P.currentRlist);
                                   P.currentRlist = NULL;
                                }
                               else
                                 {
                                    DeleteRvalItem(P.rval);
                                 }
                            };

/* HvB End Bundle Syntax */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typeid:                IDSYNTAX
                       {
                           /*
                           HvBDebug("\tP:%s:%s:%s \n",P.block, P.blocktype, P.blockid);
                           */
                           DeleteRlist(P.useargs);
                           P.useargs = NULL;
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

blockid:               BLOCK_IDSYNTAX 
                       {
                           P.offsets.last_block_id = P.offsets.last_id;
                           HvBDebug("P:%s:%s:%s blockid\n",P.block,P.blocktype,P.blockid);
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Body Section
*/
body:              body_syntax
                   {
                    /*
                    printf("\t body %s %s %s\n", $<text>4, $<text>3, $<text>2 );
                    */
                   }

body_syntax:       BODY body_type body_id body_body
                 | BODY body_type body_id usearglist body_body

body_type:         body_type_values
                   {
                           DebugBanner("Body");
                           P.block = "body";
                           DeleteRlist(P.currentRlist);
                           P.currentRlist = NULL;
                           P.currentstring = NULL;
                           strncpy(P.currenttype, "", CF_MAXVARSIZE);

                           strncpy(P.blocktype, yytext, CF_MAXVARSIZE);
                           strcpy(P.blockid, "");

                           HvBDebug("P:%s body blocktype = %s \n\n", P.block, P.blocktype);


                    }

body_type_values:     COMMON 
                    | AGENT
                    | EDITLINE
                    | typeid

body_id:           body_id_syntax
                   {
                     HvBDebug("\tP:%s:%s body blockid: %s\n", P.block, P.blocktype, yytext);
                     strncpy(P.blockid, yytext, CF_MAXVARSIZE);
                   }

body_id_syntax:      typeid
                   | blockid
                   | error
                     {
                        yyerror_hvb("Invalid body id indentifier");
                     }
 
body_body:          BLOCK_OPEN
                    {
                           HvBDebug("\tP: Body Block open\n");
                           P.currentbody = AppendBody(P.policy, P.blockid, P.blocktype, P.useargs, P.filename);
                           if (P.currentbody)
                           {
                               P.currentbody->offset.line = P.line_no;
                               P.currentbody->offset.start = P.offsets.last_block_id;
                           }

                           DeleteRlist(P.useargs);
                           P.useargs = NULL;

                           strcpy(P.currentid,"");
                     }

                     body_statements

                     BLOCK_CLOSE 
                     {
                         P.offsets.last_id = -1;
                         P.offsets.last_string = -1;
                         P.offsets.last_class_id = -1;
                         if (P.currentbody)
                         {
                             P.currentbody->offset.end = P.offsets.current;
                         }
                         HvBDebug("P: End  body block\n");
                     }


body_statements:       body_statement               
                     | body_statements body_statements;


body_statement:      class
                     | selections;

selections:            selection          
                     | selections selection;

selection:             selection_id { printf("yesy\n"); }
                       ASSIGN
                       {
                           HvBDebug("\tP:ASSIGN\n");
                       }
                       rval_type
                       {
                           CheckSelection(P.blocktype, P.blockid, P.lval, P.rval);

                           if (!INSTALL_SKIP)
                           {
                               Constraint *cp = NULL;

                               if (P.currentclasses == NULL)
                               {
                                   HvBDebug("\tAdd Constraint to body: %s\n\n", P.blockid);
                                   cp = ConstraintAppendToBody(P.currentbody, P.lval, P.rval, "any", P.references_body);
                               }
                               else
                               {
                                   HvBDebug("\tAdd Constraint to body: %s for class %s\n", P.blockid, P.currentclasses);
                                   cp = ConstraintAppendToBody(P.currentbody,P.lval,P.rval,P.currentclasses,P.references_body);
                               }

                               cp->offset.line = P.line_no;
                               cp->offset.start = P.offsets.last_id;
                               cp->offset.end = P.offsets.current;
                               cp->offset.context = P.offsets.last_class_id;
                           }
                           else
                           {
                               DeleteRvalItem(P.rval);
                           }

                           if (strcmp(P.blockid,"control") == 0 && strcmp(P.blocktype,"file") == 0)
                           {
                               if (strcmp(P.lval,"namespace") == 0)
                               {
                                   if (P.rval.rtype != CF_SCALAR)
                                   {
                                       yyerror("namespace must be a constant scalar string");
                                   }
                                   else
                                   {
                                       PolicySetNameSpace(P.policy, P.rval.item);
                                   }
                               }
                           }
                           
                           if (strcmp(P.blockid,"control") == 0 && strcmp(P.blocktype,"common") == 0)
                           {
                               if (strcmp(P.lval,"inputs") == 0)
                               {
                                   if (IsDefinedClass(P.currentclasses, CurrentNameSpace(P.policy)))
                                   {
                                       if (VINPUTLIST == NULL)
                                       {
                                           if (P.rval.rtype == CF_LIST)
                                           {
                                               VINPUTLIST = P.rval.item;
                                           }
                                           else
                                           {
                                               yyerror("inputs promise must have a list as rvalue");
                                           }
                                       }
                                       else
                                       {
                                           yyerror("Redefinition of input list (broken promise)");
                                       }
                                   }
                               }
                           }

                           P.rval = (Rval) { NULL, '\0' };
                        }
                       ';'


selection_id:          id
                       {
                           if ( !BodyTypeSyntaxLookup(P.blocktype, P.blockid, yytext) )
                           {
                              sprintf(error_txt,"%s is invalid for 'body %s %s'", 
                                      yytext, P.blocktype, P.blockid);
                               yyerror_hvb(error_txt);
                           }
                       }


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Used by body and bundle */

class:                 CLASS
                       {
                           P.offsets.last_class_id = P.offsets.current - strlen(P.currentclasses) - 2;
                           HvBDebug("\tP:%s New class contexts\n", P.currentclasses);
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

id:                    id_types
                       {
                           strncpy(P.lval, yytext, CF_MAXVARSIZE);
                           DeleteRlist(P.currentRlist);
                           P.currentRlist = NULL;
                           HvBDebug("\tP:%s:%s:%s:%s lval for '%s'\n", P.block, P.blocktype, P.blockid, P.currenttype, P.lval);
                       };

id_types:             IDSYNTAX
                    | EDITLINE  


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


rval_type:            /*  These token can never be RVAL HvB
                       IDSYNTAX
                       {
                           P.rval = (Rval) { xstrdup(P.currentid), CF_SCALAR };
                           P.references_body = true;
                           HvBDebug("\tP:%s:%s:%s:%s id RVAL '%s'\n", 
                               P.block, P.blocktype, P.blockid, P.currenttype, P.currentstring);
                       }
                     | BLOCKID
                       {
                           P.rval = (Rval) { xstrdup(P.currentid), CF_SCALAR };
                           P.references_body = true;
                           HvBDebug("\tP:%s:%s:%s:%s blockid RVAL '%s'\n", 
                              P.block, P.blocktype, P.blockid, P.currenttype, P.currentstring);
                       }
                       */
                     QSTRING
                       {
                           HvBDebug("\tP:%s:%s:%s:%s scalar RVAL '%s'\n",  
                               P.block, P.blocktype, P.blockid, P.currenttype, P.currentstring);

                           P.rval = (Rval) { P.currentstring, CF_SCALAR };

                           P.currentstring = NULL;
                           P.references_body = false;

                           if (P.currentpromise)
                           {
                               if (LvalWantsBody(P.currentpromise->agentsubtype,P.lval))
                               {
                                   yyerror("An rvalue is quoted, but we expect an unquoted body identifier");
                               }
                           }
                       }
                     | NAKEDVAR
                       {
                           P.rval = (Rval) { P.currentstring, CF_SCALAR };
                           HvBDebug("\tP:%s:%s:%s:%s scalarvariable RVAL '%s'\n", 
                               P.block, P.blocktype, P.blockid, P.currenttype, P.currentstring);

                           P.currentstring = NULL;
                           P.references_body = false;
                       }
                     | list
                       {
                           P.rval = (Rval) { CopyRlist(P.currentRlist), CF_LIST };
                           HvBDebug("\tP:%s:%s:%s:%s list  RVAL\n", 
                               P.block, P.blocktype, P.blockid, P.currenttype);
                           DeleteRlist(P.currentRlist);
                           P.currentRlist = NULL;
                           P.references_body = false;
                       }
                     | usefunction
                       {
                           HvBDebug("\tP:%s:%s:%s:%s usefunction  RVAL '%s'\n", 
                               P.block, P.blocktype, P.blockid, P.currenttype, P.currentstring);
                           P.rval = (Rval) { P.currentfncall[P.arg_nesting+1], CF_FNCALL };
                           P.references_body = false;
                       }
                     | usefunction_noargs
                       {
                           HvBDebug("\tP:%s:%s:%s:%s usefunction  with no args RVAL '%s'\n", 
                               P.block, P.blocktype, P.blockid, P.currenttype, P.currentstring);
                               /*
                           P.rval = (Rval) { P.currentfncall[P.arg_nesting+1], CF_FNCALL };
                           P.references_body = false;
                              */
                           P.rval = (Rval) { xstrdup(P.currentid), CF_SCALAR };
                           P.references_body = true;
                           HvBDebug("\tP:%s:%s:%s:%s id RVAL '%s'\n", 
                               P.block, P.blocktype, P.blockid, P.currenttype, P.currentstring);
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

list:                  BLOCK_OPEN { printf("\tP:list open\n") ; }
                       litems
                       BLOCK_CLOSE { printf("\tP:list closed\n"); }
                       ; 

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

litems:                litems_int
                     | litems_int ',';

litems_int:            litem
                     | litems_int ',' litem;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

litem:                 IDSYNTAX
                       {
                           HvBDebug("\tP:%s:%s:%s added idsyntax item = %s\n", P.block,P.blocktype,P.blockid,  yytext);
                           AppendRlist((Rlist **)&P.currentRlist,P.currentid,CF_SCALAR);
                       }

                    |  QSTRING
                       {
                           HvBDebug("\tP:%s:%s:%s added qstring item = %s\n", P.block,P.blocktype,P.blockid,  yytext);
                           AppendRlist((Rlist **)&P.currentRlist,(void *)P.currentstring,CF_SCALAR);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | NAKEDVAR
                       {
                           HvBDebug("\tP:%s:%s:%s added nakedvart item\n", P.block,P.blocktype,P.blockid);
                           AppendRlist((Rlist **)&P.currentRlist,(void *)P.currentstring,CF_SCALAR);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | usefunction
                       {
                           HvBDebug("\tP: Install function call as list item from level %d\n",P.arg_nesting+1);
                           AppendRlist((Rlist **)&P.currentRlist,(void *)P.currentfncall[P.arg_nesting+1],CF_FNCALL);
                           DeleteFnCall(P.currentfncall[P.arg_nesting+1]);
                       }
                     | usefunction_noargs
                       {
                           HvBDebug("\tP: Install function call with no args as list item from level %d\n",P.arg_nesting+1);
                           AppendRlist((Rlist **)&P.currentRlist,(void *)P.currentfncall[P.arg_nesting+1],CF_FNCALL);
                           DeleteFnCall(P.currentfncall[P.arg_nesting+1]);
                       }


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

functionid:            IDSYNTAX
                       {
                           HvBDebug("\tP:%s:%s:%s:%s Found function identifier '%s'\n", 
                              P.block, P.blocktype, P.blockid, P.currenttype, P.currentid);
                       }
                     | BLOCK_IDSYNTAX
                       {
                           HvBDebug("\tP:%s:%s:%s:%s  Found qualified function identifier '%s'\n", 
                              P.block, P.blocktype, P.blockid, P.currenttype, P.currentid);
                       } 
                     | NAKEDVAR
                       {
                           strncpy(P.currentid,P.currentstring,CF_MAXVARSIZE); // Make a var look like an ID
                           free(P.currentstring);
                           P.currentstring = NULL;
                           HvBDebug("P: Found variable in place of a function identifier %s\n",P.currentid);
                       }
                     | error
                       {
                          yyerror_hvb("Not a valid function indentifier\n");
                       }
                       

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

promiser:              QSTRING
                       {
                           P.promiser = P.currentstring;
                           P.currentstring = NULL;

                           HvBDebug("\tP:%s:%s:%s:%s:%s Promising object name \'%s\'\n", P.block, P.blocktype, P.blockid, P.currenttype, P.currentclasses, P.promiser);
                            

                           P.currentpromise = AppendPromise(
                              P.currentstype, P.promiser, 
                              (Rval) { NULL, CF_NOPROMISEE },
                              P.currentclasses ? P.currentclasses : "any", 
                              P.blockid, P.blocktype,CurrentNameSpace(P.policy)
                            ); 

                            P.currentpromise->offset.line = P.line_no;
                            P.currentpromise->offset.start = P.offsets.last_string;
                            P.currentpromise->offset.context = P.offsets.last_class_id;
                            
                       };


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

usefunction_noargs:    functionid
                       {
                           HvBDebug("\tP: Finished with function call %s, now at level %d\n", P.currentid, P.arg_nesting);
                           /*
                           P.currentfnid[++P.arg_nesting] = xstrdup(P.currentid);
                           P.currentfncall[P.arg_nesting] = NewFnCall(P.currentfnid[P.arg_nesting],P.giveargs[P.arg_nesting]);
                           P.giveargs[P.arg_nesting] = NULL;
                           strcpy(P.currentid,"");
                           free(P.currentfnid[P.arg_nesting]);
                           P.currentfnid[P.arg_nesting] = NULL;
                           P.arg_nesting--;
                           */
                       }

usefunction:           functionid givearglist
                       {
                           /* Careful about recursion */
                           HvBDebug("\tP: Finished with function call, now at level %d\n",P.arg_nesting);
                       }
                       | error
                         {
                            yyerror_hvb("Error in function definition ");
                         }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

givearglist:           '('
                       {
                           if (++P.arg_nesting >= CF_MAX_NESTING)
                           {
                               fatal_yyerror("Nesting of functions is deeper than recommended");
                           }
                           HvBDebug("\tP: Start FnCall %s args level %d\n",P.currentid, P.arg_nesting);
                           P.currentfnid[P.arg_nesting] = xstrdup(P.currentid);
                           HvBDebug("\tP: Start FnCall %s args level %d\n",P.currentfnid[P.arg_nesting],P.arg_nesting);
                       }

                       gaitems

                       ')'
                       {
                           HvBDebug("\tP: End args level %d\n",P.arg_nesting);
                           P.currentfncall[P.arg_nesting] = NewFnCall(P.currentfnid[P.arg_nesting],P.giveargs[P.arg_nesting]);
                           P.giveargs[P.arg_nesting] = NULL;
                           strcpy(P.currentid,"");
                           free(P.currentfnid[P.arg_nesting]);
                           P.currentfnid[P.arg_nesting] = NULL;
                           P.arg_nesting--;
                           HvBDebug("\tP: End args level %d\n",P.arg_nesting);
                       }


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

gaitems:               gaitem
                     | gaitems ',' gaitem
                     |; 

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

gaitem:                IDSYNTAX
                       {
                           /* currently inside a use function */
                           HvBDebug("\tFnCall arg: %s\n", P.currentid);
                           AppendRlist(&P.giveargs[P.arg_nesting],P.currentid,CF_SCALAR);
                       }

                     | QSTRING
                       {
                           /* currently inside a use function */
                           HvBDebug("\tFnCall arg: %s\n", P.currentstring);
                           AppendRlist(&P.giveargs[P.arg_nesting],P.currentstring,CF_SCALAR);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | NAKEDVAR
                       {
                           /* currently inside a use function */
                           HvBDebug("\tFnCall arg: %s\n", P.currentstring);
                           AppendRlist(&P.giveargs[P.arg_nesting],P.currentstring,CF_SCALAR);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | usefunction
                       {
                           /* Careful about recursion */
                           HvBDebug("\tFnCall new function\n");
                           AppendRlist(&P.giveargs[P.arg_nesting],(void *)P.currentfncall[P.arg_nesting+1],CF_FNCALL);
                           DeleteRvalItem((Rval) { P.currentfncall[P.arg_nesting+1], CF_FNCALL });
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

usearglist:            '('
                       aitems
                       ')';

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

aitems:                aitem
                     | aitems ',' aitem
                     |;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

aitem:                 aitem_type  /* recipient of argument is never a literal */
                       {
                           HvBDebug("P:%s:%s:%s added arg = %s\n", P.block,P.blocktype,P.blockid,  yytext);
                           AppendRlist(&(P.useargs),yytext,CF_SCALAR);
                       };

aitem_type:            IDSYNTAX
       

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

%%

/*****************************************************************/

static bool BodyTypeSyntaxLookup(const char *body_type, const char *block_id, char *name)
{
   SubTypeSyntax *ss;
   BodySyntax    *bs, *bs2;
   int           i,j,l,k;


/*
   printf("HvB %s %s %s\n", body_type, block_id, name);
*/

   if ( strcmp(block_id, "control") == 0 )
   {
      if ( strcmp(body_type, "common") == 0 )
      {
         bs = (BodySyntax *)CFG_CONTROLBODY;
      }
      else if ( strcmp(body_type, "agent") == 0 )
      {
         bs = (BodySyntax *)CFA_CONTROLBODY;
      }
      else if ( strcmp(body_type, "executor") == 0 )
      {
         bs = (BodySyntax *)CFEX_CONTROLBODY;
      }
      else if ( strcmp(body_type, "server") == 0 )
      {
         bs = (BodySyntax *)CFS_CONTROLBODY;
      }
      else
      {
         bs = NULL;
      }

      for ( i=0; bs[i].lval != NULL; i++)
      {   
          if ( strcmp(bs[i].lval, name) == 0 )
          {
             return true;
          }
      }
      if ( bs != NULL )
      {
         return false;
      }
    }

/* debug purpose
   for (i = 0; i < CF3_MODULES; i++)
   {
       if ((ss = (SubTypeSyntax *)CF_ALL_SUBTYPES[i]) == NULL)
       {
           continue;
       }
       for (j = 0; ss[j].subtype != NULL; j++)
       {
           printf("Examining subtype %s\n", ss[j].subtype);
           printf("\t bundle_type %s\n", ss[j].bundle_type);
           
           if ((bs = (BodySyntax *)ss[j].bs) == NULL)
           {
               continue;
           }

          for (l = 0; bs[l].range != NULL; l++)
          {
              printf("\tbs[l].lval  %s\n", bs[l].lval);
              printf("\tbs[l].dtype %d\n", bs[l].dtype );

              bs2 = (BodySyntax *) (bs[l].range);

              if ( bs2 == NULL)
                  continue;

              if ( bs[l].dtype != cf_body )
                  continue;

              printf("\t\tbs2[0].description = %s\n", bs2[0].description);

              printf("\t\tbs[k]\n");
              for (k = 0; bs2[k].lval != NULL; k++)
              {  
                  printf("\t\tbs2[k].lval = %s\n", bs2[k].lval);
              }

          }
        }
          exit(1);
   }
*/

  
   for (i = 0; i < CF3_MODULES; i++)
   {
   /*
       printf("Trying function module %d for matching lval %s\n", i, name);
   */

       if ((ss = (SubTypeSyntax *)CF_ALL_SUBTYPES[i]) == NULL)
       {
           continue;
       }

       for (j = 0; ss[j].subtype != NULL; j++)
       {
       /*
           printf("Examining subtype %s\n", ss[j].subtype);
           printf("\t bundle_type %s\n", ss[j].bundle_type);
       */
           
           if ((bs = (BodySyntax *)ss[j].bs) == NULL)
           {
               continue;
           }

          for (l = 0; bs[l].range != NULL; l++)
          {
              /*
              printf("\tbs[l].lval  %s\n", bs[l].lval);
              printf("\tbs[l].dtype %d\n", bs[l].dtype );
              */

              if ( (bs[l].dtype == cf_body)) 
              {

/*
                 if (bs2 == NULL || bs2 == (void *) CF_BUNDLE)
 */
                 bs2 = (const BodySyntax *) (bs[l].range);

                 if (bs2 == NULL ) 
                 {
                    continue;
                 }

                 if ( (strcmp(body_type, bs[l].lval) == 0) || (strcmp("action", bs[l].lval) == 0) )
                 {
                 /*
                     printf("We found a type match for %s:%s\n", P.blocktype, bs[l].lval);
                 */

                     for (k = 0; bs2[k].dtype != cf_notype; k++)
                     {  
                         if (strcmp(bs2[k].lval, name) == 0 )
                         { 
                             /*
                             printf("We found a match for %s:%s\n", bs2[k].lval, name);
                             */
                             return true;
                         }
                      }
                   /*
                   return false;
                   */

                   }
                } 
            } /* end for l */
          } /* end for j */
     } /* end for i */
     return false;
}

/*****************************************************************/

void yyerror(const char *s)
{

/*
    char *sp = yytext;

    if (sp == NULL)
    {
        if (USE_GCC_BRIEF_FORMAT)
        {
            fprintf(stderr, "%s:%d:%d: error: %s\n", P.filename, P.line_no, P.line_pos, s);
        }
        else
        {
            fprintf(stderr, "%s> %s:%d,%d: %s, near token \'NULL\'\n", VPREFIX, P.filename, P.line_no, P.line_pos, s);
        }
    }
    else if (*sp == '\"' && strlen(sp) > 1)
    {
        sp++;
    }

    if (USE_GCC_BRIEF_FORMAT)
    {
        fprintf(stderr, "%s:%d:%d: error: %s, near token \'%.20s\'\n", P.filename, P.line_no, P.line_pos, s, sp);
    }
    else
    {
        fprintf(stderr, "%s> %s:%d,%d: %s, near token \'%.20s\'\n", VPREFIX, P.filename, P.line_no, P.line_pos, s, sp);
    }

    ERRORCOUNT++;

    if (ERRORCOUNT > 10)
    {
        FatalError("Too many errors");
    }
    yyerror_hvb(s);
    */
}

static void fatal_yyerror(const char *s)
{
    char *sp = yytext;
    /* Skip quotation mark */
    if (sp && *sp == '\"' && sp[1])
    {
        sp++;
    }

    FatalError("%s: %d,%d: Fatal error during parsing: %s, near token \'%.20s\'\n", P.filename, P.line_no, P.line_pos, s, sp ? sp : "NULL");
}

static void DebugBanner(const char *s)
{
    HvBDebug("----------------------------------------------------------------\n");
    HvBDebug("  %s                                                            \n", s);
    HvBDebug("----------------------------------------------------------------\n");
}

static void yyerror_hvb(const char *s)
{
    int i;

    /*
     * change tabs into spaces
    */
    for ( i = 0; i < strlen(cf_linebuf); i++ )
    if ( cf_linebuf[i] == '\t' )
        cf_linebuf[i] = ' ';

    /*
     * Substracts the last token length else we point to the wrong token
     * in the line.
    */
    i = yyleng;
    cf_tokenpos -= i;
    /*
    printf("hvb cf_tokenpos = %d, %d\n", cf_tokenpos, i);
    */

    /*
     * Display the error message in the following format
     *  line 2 : syntax error
     *    ConfigFile = klaar/bas
     *             ^invalid pathname
    */
    fprintf(stderr, "filename: %s line %d: token:%s\n", P.filename, P.line_no, yytext);
    fprintf(stderr, "error: %s\n", s);

    fprintf(stderr, "\n%s\n", cf_linebuf);
    fprintf(stderr, "%*s error\n", 2 + cf_tokenpos, "^");

    exit(1);
}
