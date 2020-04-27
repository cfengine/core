/*
  Copyright 2020 Northern.tech AS

  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

%{
#include <cf3parse_logic.h>
%}

%token IDSYNTAX BLOCKID QSTRING CLASS PROMISE_GUARD BUNDLE BODY ASSIGN ARROW NAKEDVAR
%expect 1

%%

specification:       /* empty */
                     | blocks

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

blocks:                block
                     | blocks block;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

block:                 bundle
                     | body
                     | error
                       {
                           ParseError("Expected 'bundle' or 'body' keyword, wrong input '%s'", yytext);
                           YYABORT;
                       }

bundle:                BUNDLE bundletype bundleid arglist bundlebody

body:                  BODY bodytype bodyid arglist bodybody

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bundletype:            bundletype_values
                       {
                           ParserBeginBundle();
                       }

bundletype_values:     typeid
                       {
                           /* FIXME: We keep it here, because we skip unknown
                            * promise bundles. Ought to be moved to
                            * after-parsing step once we know how to deal with
                            * it */

                           if (!BundleTypeCheck(P.blocktype))
                           {
                               ParseError("Unknown bundle type '%s'", P.blocktype);
                               INSTALL_SKIP = true;
                           }
                       }
                     | error
                       {
                           yyclearin;
                           ParseError("Expected bundle type, wrong input '%s'", yytext);
                           INSTALL_SKIP = true;
                       }

bundleid:              bundleid_values
                       {
                          ParserDebug("\tP:bundle:%s:%s\n", P.blocktype, P.blockid);
                          CURRENT_BLOCKID_LINE = P.line_no;
                       }

bundleid_values:       symbol
                     | error
                       {
                           yyclearin;
                           ParseError("Expected bundle identifier, wrong input '%s'", yytext);
                           INSTALL_SKIP = true;
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bodytype:              bodytype_values
                       {
                           ParserBeginBody();
                       }

bodytype_values:       typeid
                       {
                           if (!BodySyntaxGet(PARSER_BLOCK_BODY, P.blocktype))
                           {
                               ParseError("Unknown body type '%s'", P.blocktype);
                           }
                       }
                     | error
                       {
                           yyclearin;
                           ParseError("Expected body type, wrong input '%s'", yytext);
                       }

bodyid:                bodyid_values
                       {
                          ParserDebug("\tP:body:%s:%s\n", P.blocktype, P.blockid);
                          CURRENT_BLOCKID_LINE = P.line_no;
                       }

bodyid_values:         symbol
                     | error
                       {
                           yyclearin;
                           ParseError("Expected body identifier, wrong input '%s'", yytext);
                           INSTALL_SKIP = true;
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typeid:                IDSYNTAX
                       {
                           strncpy(P.blocktype,P.currentid,CF_MAXVARSIZE);

                           RlistDestroy(P.useargs);
                           P.useargs = NULL;
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

symbol:                IDSYNTAX
                       {
                           strncpy(P.blockid,P.currentid,CF_MAXVARSIZE);
                           P.offsets.last_block_id = P.offsets.last_id;
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

arglist:               /* Empty */
                     | arglist_begin aitems arglist_end
                     | arglist_begin arglist_end
                     | arglist_begin error
                       {
                          yyclearin;
                          ParseError("Error in bundle parameter list, expected ')', wrong input '%s'", yytext);
                       }

arglist_begin:         '('
                       {
                           ParserDebug("P:%s:%s:%s arglist begin:%s\n", ParserBlockString(P.block),P.blocktype,P.blockid, yytext);
                       }

arglist_end:           ')'
                       {
                           ParserDebug("P:%s:%s:%s arglist end:%s\n", ParserBlockString(P.block),P.blocktype,P.blockid, yytext);
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

aitems:                aitem
                     | aitems ',' aitem

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

aitem:                 IDSYNTAX  /* recipient of argument is never a literal */
                       {
                           ParserDebug("P:%s:%s:%s  arg id: %s\n", ParserBlockString(P.block),P.blocktype,P.blockid, P.currentid);
                           RlistAppendScalar(&(P.useargs),P.currentid);
                       }
                     | error
                       {
                          yyclearin;
                          ParseError("Expected identifier, wrong input '%s'", yytext);
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bundlebody:            body_begin
                       {
                           ParserBeginBundleBody();
                       }

                       bundle_decl

                       '}'
                       {
                           INSTALL_SKIP = false;
                           ParserEndCurrentBlock();
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

body_begin:            '{'
                       {
                           ParserDebug("P:%s:%s:%s begin body open\n", ParserBlockString(P.block),P.blocktype,P.blockid);
                       }
                     | error
                       {
                           ParseError("Expected body open '{', wrong input '%s'", yytext);
                       }


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bundle_decl:           /* empty */
                     | bundle_statements

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bundle_statements:     bundle_statement
                     | bundle_statements bundle_statement
                     | error
                       {
                          INSTALL_SKIP = true;
                          ParseError("Expected promise type, got '%s'", yytext);
                          ParserDebug("P:promise_type:error yychar = %d, %c, yyempty = %d\n", yychar, yychar, YYEMPTY);
                          yyclearin;
                       }


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bundle_statement:      promise_guard classpromises_decl

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

promise_guard:         PROMISE_GUARD             /* BUNDLE ONLY */
                       {
                           ParserHandlePromiseGuard();
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

classpromises_decl:    /* empty */
                     | classpromises

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

classpromises:         classpromise
                     | classpromises classpromise

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

classpromise:          class
                     | promise_decl

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


promise_decl:          promise_line ';'
                     | promiser error
                       {
                           /*
                            * Based on yychar display right error message
                           */
                           ParserDebug("P:promiser:error yychar = %d\n", yychar);
                           if (yychar =='-' || yychar == '>')
                           {
                              ParseError("Expected '->', got '%s'", yytext);
                           }
                           else if (yychar == IDSYNTAX)
                           {
                              ParseError("Expected attribute, got '%s'", yytext);
                           }
                           else if (yychar == ',')
                           {
                              ParseError("Expected attribute, got '%s' (comma after promiser is not allowed since 3.5.0)", yytext);
                           }
                           else
                           {
                              ParseError("Expected ';', got '%s'", yytext);
                           }
                           yyclearin;
                       }

promise_line:           promisee_statement
                      | promiser_statement


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

promisee_statement:    promiser

                       arrow_type

                       rval
                       {
                           if (!INSTALL_SKIP)
                           {
                               if (!P.currentstype)
                               {
                                   ParseError("Missing promise type declaration");
                               }

                               P.currentpromise = PromiseTypeAppendPromise(P.currentstype, P.promiser,
                                                                           RvalCopy(P.rval),
                                                                           P.currentclasses ? P.currentclasses : "any",
                                                                           P.currentvarclasses);
                               P.currentpromise->offset.line = CURRENT_PROMISER_LINE;
                               P.currentpromise->offset.start = P.offsets.last_string;
                               P.currentpromise->offset.context = P.offsets.last_class_id;
                           }
                           else
                           {
                               P.currentpromise = NULL;
                           }
                       }

                       promiser_constraints_decl

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

promiser_statement:    promiser
                       {

                           if (!INSTALL_SKIP)
                           {
                               if (!P.currentstype)
                               {
                                   ParseError("Missing promise type declaration");
                               }

                               P.currentpromise = PromiseTypeAppendPromise(P.currentstype, P.promiser,
                                                                (Rval) { NULL, RVAL_TYPE_NOPROMISEE },
                                                                           P.currentclasses ? P.currentclasses : "any",
                                                                           P.currentvarclasses);
                               P.currentpromise->offset.line = CURRENT_PROMISER_LINE;
                               P.currentpromise->offset.start = P.offsets.last_string;
                               P.currentpromise->offset.context = P.offsets.last_class_id;
                           }
                           else
                           {
                               P.currentpromise = NULL;
                           }
                       }

                       promiser_constraints_decl

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

promiser:              QSTRING
                       {
                           if (P.promiser)
                           {
                               free(P.promiser);
                           }
                           P.promiser = P.currentstring;
                           P.currentstring = NULL;
                           CURRENT_PROMISER_LINE = P.line_no;
                           ParserDebug("\tP:%s:%s:%s:%s:%s promiser = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currenttype, P.currentclasses ? P.currentclasses : "any", P.promiser);
                       }
                     | error
                       {
                          INSTALL_SKIP = true;
                          ParserDebug("P:promiser:qstring::error yychar = %d\n", yychar);

                          if (yychar == BUNDLE || yychar == BODY)
                          {
                             ParseError("Expected '}', got '%s'", yytext);
                             /*
                             YYABORT;
                             */
                          }
                          else
                          {
                             ParseError("Expected promiser string, got '%s'", yytext);
                          }

                          yyclearin;
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

promiser_constraints_decl:      /* empty */
                              | constraints_decl
                              | constraints_decl error
                                {
                                   /*
                                    * Based on next token id display right error message
                                   */
                                   ParserDebug("P:constraints_decl:error yychar = %d\n", yychar);
                                   if ( yychar == IDSYNTAX )
                                   {
                                       ParseError("Check previous line, Expected ',', got '%s'", yytext);
                                   }
                                   else
                                   {
                                       ParseError("Check previous line, Expected ';', got '%s'", yytext);
                                   }
                                   yyclearin;

                                }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


constraints_decl:      constraints
                       {
                           /* Don't free these */
                           strcpy(P.currentid,"");
                           RlistDestroy(P.currentRlist);
                           P.currentRlist = NULL;
                           free(P.promiser);
                           if (P.currentstring)
                           {
                               free(P.currentstring);
                           }
                           P.currentstring = NULL;
                           P.promiser = NULL;
                           P.promisee = NULL;
                           /* reset argptrs etc*/
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

constraints:           constraint                           /* BUNDLE ONLY */
                     | constraints ',' constraint


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

constraint:            constraint_id                        /* BUNDLE ONLY */
                       assign_type
                       rval
                       {
                           ParserHandleBundlePromiseRval();
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

constraint_id:         IDSYNTAX                        /* BUNDLE ONLY */
                       {
                           ParserDebug("\tP:%s:%s:%s:%s:%s:%s attribute = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currenttype, P.currentclasses ? P.currentclasses : "any", P.promiser, P.currentid);

                           const PromiseTypeSyntax *promise_type_syntax = PromiseTypeSyntaxGet(P.blocktype, P.currenttype);
                           if (!promise_type_syntax)
                           {
                               ParseError("Invalid promise type '%s' in bundle '%s' of type '%s'", P.currenttype, P.blockid, P.blocktype);
                               INSTALL_SKIP = true;
                           }
                           else if (!PromiseTypeSyntaxGetConstraintSyntax(promise_type_syntax, P.currentid))
                           {
                               ParseError("Unknown attribute '%s' for promise type '%s' in bundle with type '%s'", P.currentid, P.currenttype, P.blocktype);
                               INSTALL_SKIP = true;
                           }

                           strncpy(P.lval,P.currentid,CF_MAXVARSIZE);
                           RlistDestroy(P.currentRlist);
                           P.currentRlist = NULL;
                       }
                     | error
                       {
                             ParseError("Expected attribute, got '%s'\n", yytext);
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bodybody:              body_begin
                       {
                           ParserBeginBlockBody();
                       }

                       bodyattribs

                       '}'
                       {
                           ParserEndCurrentBlock();
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bodyattribs:           bodyattrib                    /* BODY ONLY */
                     | bodyattribs bodyattrib

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bodyattrib:            class
                     | selection_line

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

selection_line:        selection ';'
                     | selection error
                       {
                          ParseError("Expected ';' check previous statement, got '%s'", yytext);
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

selection:             selection_id                         /* BODY ONLY */
                       assign_type
                       rval
                       {
                           ParserHandleBlockAttributeRval();
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

selection_id:          IDSYNTAX
                       {
                           ParserDebug("\tP:%s:%s:%s:%s attribute = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentid);

                           if (!INSTALL_SKIP)
                           {
                               const BodySyntax *body_syntax = BodySyntaxGet(PARSER_BLOCK_BODY, P.currentbody->type);

                               if (!body_syntax || !BodySyntaxGetConstraintSyntax(body_syntax->constraints, P.currentid))
                               {
                                   ParseError("Unknown selection '%s' for body type '%s'", P.currentid, P.currentbody->type);
                                   INSTALL_SKIP = true;
                               }

                               strncpy(P.lval,P.currentid,CF_MAXVARSIZE);
                           }
                           RlistDestroy(P.currentRlist);
                           P.currentRlist = NULL;
                       }
                     | error
                       {
                          ParserDebug("P:selection_id:idsyntax:error yychar = %d\n", yychar);

                          if ( yychar == BUNDLE || yychar == BODY )
                          {
                             ParseError("Expected '}', got '%s'", yytext);
                             /*
                             YYABORT;
                             */
                          }
                          else
                          {
                             ParseError("Expected attribute, got '%s'", yytext);
                          }

                          yyclearin;
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

assign_type:           ASSIGN
                       {
                           ParserDebug("\tP:=>\n");
                       }
                     | error
                       {
                          yyclearin;
                          ParseError("Expected '=>', got '%s'", yytext);
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

arrow_type:            ARROW
                       {
                           ParserDebug("\tP:->\n");
                       }
                       /* else we display the wrong error
                     | error
                       {
                          yyclearin;
                          ParseError("Expected '->', got '%s'", yytext);
                       }
                       */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class:                 CLASS
                       {
                           P.offsets.last_class_id = P.offsets.current - strlen(P.currentclasses ? P.currentclasses : P.currentvarclasses) - 2;
                           ParserDebug("\tP:%s:%s:%s:%s %s = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currenttype, P.currentclasses ? "class": "varclass", yytext);

                           if (P.currentclasses != NULL)
                           {
                               char *literal = xstrdup(P.currentclasses);

                               ValidateClassLiteral(literal);

                               free(literal);
                           }
                       }
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

rval:                  IDSYNTAX
                       {
                           ParserDebug("\tP:%s:%s:%s:%s id rval, %s = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.lval, P.currentid);
                           RvalDestroy(P.rval);
                           P.rval = (Rval) { xstrdup(P.currentid), RVAL_TYPE_SCALAR };
                           P.references_body = true;
                       }
                     | BLOCKID
                       {
                           ParserDebug("\tP:%s:%s:%s:%s blockid rval, %s = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.lval, P.currentid);
                           RvalDestroy(P.rval);
                           P.rval = (Rval) { xstrdup(P.currentid), RVAL_TYPE_SCALAR };
                           P.references_body = true;
                       }
                     | QSTRING
                       {
                           ParserDebug("\tP:%s:%s:%s:%s qstring rval, %s = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.lval, P.currentstring);
                           RvalDestroy(P.rval);
                           P.rval = (Rval) { P.currentstring, RVAL_TYPE_SCALAR };

                           P.currentstring = NULL;
                           P.references_body = false;

                           if (P.currentpromise)
                           {
                               if (LvalWantsBody(P.currentpromise->parent_promise_type->name, P.lval))
                               {
                                   yyerror("An rvalue is quoted, but we expect an unquoted body identifier");
                               }
                           }
                       }
                     | NAKEDVAR
                       {
                           ParserDebug("\tP:%s:%s:%s:%s nakedvar rval, %s = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.lval, P.currentstring);
                           RvalDestroy(P.rval);
                           P.rval = (Rval) { P.currentstring, RVAL_TYPE_SCALAR };

                           P.currentstring = NULL;
                           P.references_body = false;
                       }
                     | list
                       {
                           ParserDebug("\tP:%s:%s:%s:%s install list =  %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.lval);
                           RvalDestroy(P.rval);
                           P.rval = (Rval) { RlistCopy(P.currentRlist), RVAL_TYPE_LIST };
                           RlistDestroy(P.currentRlist);
                           P.currentRlist = NULL;
                           P.references_body = false;
                       }
                     | usefunction
                       {
                           RvalDestroy(P.rval);
                           P.rval = (Rval) { P.currentfncall[P.arg_nesting+1], RVAL_TYPE_FNCALL };
                           P.currentfncall[P.arg_nesting+1] = NULL;
                           P.references_body = false;
                       }

                     | error
                       {
                           yyclearin;
                           ParseError("Invalid r-value type '%s'", yytext);
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

list:                  '{' '}'
                     | '{' litems '}'
                     | '{' litems ',' '}'

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

litems:
                       litem
                     | litems ',' litem
                     | litem error
                       {
                           ParserDebug("P:rval:list:error yychar = %d\n", yychar);
                           if ( yychar ==';' )
                           {
                               ParseError("Expected '}', wrong input '%s'", yytext);
                           }
                           else if ( yychar == ASSIGN )
                           {
                               ParseError("Check list statement previous line,"
                                          " Expected '}', wrong input '%s'",
                                          yytext);
                           }
                           else
                           {
                               ParseError("Expected ',', wrong input '%s'", yytext);
                           }
                           yyclearin;
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

litem:                 IDSYNTAX
                       {
                           ParserDebug("\tP:%s:%s:%s:%s list append: "
                                       "id = %s\n",
                                       ParserBlockString(P.block), P.blocktype, P.blockid,
                                       (P.currentclasses ?
                                            P.currentclasses : "any"),
                                       P.currentid);
                           RlistAppendScalar((Rlist **) &P.currentRlist,
                                             P.currentid);
                       }

                     | QSTRING
                       {
                           ParserDebug("\tP:%s:%s:%s:%s list append: "
                                       "qstring = %s\n",
                                       ParserBlockString(P.block), P.blocktype, P.blockid,
                                       (P.currentclasses ?
                                            P.currentclasses : "any"),
                                       P.currentstring);
                           RlistAppendScalar((Rlist **) &P.currentRlist,
                                             (void *) P.currentstring);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | NAKEDVAR
                       {
                           ParserDebug("\tP:%s:%s:%s:%s list append: nakedvar = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentstring);
                           RlistAppendScalar((Rlist **)&P.currentRlist,(void *)P.currentstring);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | usefunction
                       {
                           RlistAppend(&P.currentRlist, P.currentfncall[P.arg_nesting+1], RVAL_TYPE_FNCALL);
                           FnCallDestroy(P.currentfncall[P.arg_nesting+1]);
                           P.currentfncall[P.arg_nesting+1] = NULL;
                       }

                     | error
                       {
                          yyclearin;
                          ParseError("Invalid input for a list item, got '%s'", yytext);
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

functionid:            IDSYNTAX
                       {
                           ParserDebug("\tP:%s:%s:%s:%s function id = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentid);
                       }
                     | BLOCKID
                       {
                           ParserDebug("\tP:%s:%s:%s:%s function blockid = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentid);
                       }
                     | NAKEDVAR
                       {
                           ParserDebug("\tP:%s:%s:%s:%s function nakedvar = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentstring);
                           strncpy(P.currentid,P.currentstring,CF_MAXVARSIZE); // Make a var look like an ID
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

usefunction:           functionid givearglist
                       {
                           ParserDebug("\tP:%s:%s:%s:%s Finished with function, now at level %d\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.arg_nesting);
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

givearglist:           '('
                       {
                           if (++P.arg_nesting >= CF_MAX_NESTING)
                           {
                               fatal_yyerror("Nesting of functions is deeper than recommended");
                           }
                           P.currentfnid[P.arg_nesting] = xstrdup(P.currentid);
                           ParserDebug("\tP:%s:%s:%s begin givearglist for function %s, level %d\n", ParserBlockString(P.block),P.blocktype,P.blockid, P.currentfnid[P.arg_nesting], P.arg_nesting );
                       }

                       gaitems

                       ')'
                       {
                           ParserDebug("\tP:%s:%s:%s end givearglist for function %s, level %d\n", ParserBlockString(P.block),P.blocktype,P.blockid, P.currentfnid[P.arg_nesting], P.arg_nesting );
                           P.currentfncall[P.arg_nesting] = FnCallNew(P.currentfnid[P.arg_nesting], P.giveargs[P.arg_nesting]);
                           P.giveargs[P.arg_nesting] = NULL;
                           strcpy(P.currentid,"");
                           free(P.currentfnid[P.arg_nesting]);
                           P.currentfnid[P.arg_nesting] = NULL;
                           P.arg_nesting--;
                       }


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

gaitems:               /* empty */
                     | gaitem
                     | gaitems ',' gaitem
                     | gaitem error
                       {
                           ParseError("Expected ',', wrong input '%s'", yytext);
                           yyclearin;
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

gaitem:                IDSYNTAX
                       {
                           ParserDebug("\tP:%s:%s:%s:%s function %s, id arg = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentfnid[P.arg_nesting], P.currentid);
                           /* currently inside a use function */
                           RlistAppendScalar(&P.giveargs[P.arg_nesting],P.currentid);
                       }

                     | QSTRING
                       {
                           /* currently inside a use function */
                           ParserDebug("\tP:%s:%s:%s:%s function %s, qstring arg = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentfnid[P.arg_nesting], P.currentstring);
                           RlistAppendScalar(&P.giveargs[P.arg_nesting],P.currentstring);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | NAKEDVAR
                       {
                           /* currently inside a use function */
                           ParserDebug("\tP:%s:%s:%s:%s function %s, nakedvar arg = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentfnid[P.arg_nesting], P.currentstring);
                           RlistAppendScalar(&P.giveargs[P.arg_nesting],P.currentstring);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | usefunction
                       {
                           /* Careful about recursion */
                           ParserDebug("\tP:%s:%s:%s:%s function %s, nakedvar arg = %s\n", ParserBlockString(P.block), P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentfnid[P.arg_nesting], P.currentstring);
                           RlistAppend(&P.giveargs[P.arg_nesting], P.currentfncall[P.arg_nesting+1], RVAL_TYPE_FNCALL);
                           RvalDestroy((Rval) { P.currentfncall[P.arg_nesting+1], RVAL_TYPE_FNCALL });
                           P.currentfncall[P.arg_nesting+1] = NULL;
                       }

                     | error
                       {
                           ParserDebug("P:rval:function:gaitem:error yychar = %d\n", yychar);
                           if (yychar == ';')
                           {
                              ParseError("Expected ')', wrong input '%s'", yytext);
                           }
                           else if (yychar == ASSIGN )
                           {
                              ParseError("Check function statement  previous line, Expected ')', wrong input '%s'", yytext);
                           }
                           else
                           {
                              ParseError("Invalid function argument, wrong input '%s'", yytext);
                           }
                           yyclearin;
                       }

%%
