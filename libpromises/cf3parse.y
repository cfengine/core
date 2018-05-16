
%{

/*
   Copyright 2018 Northern.tech AS

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

#include "cf3.defs.h"
#include "parser.h"
#include "parser_state.h"

#include "logging.h"
#include "fncall.h"
#include "rlist.h"
#include "item_lib.h"
#include "policy.h"
#include "mod_files.h"
#include "string_lib.h"
#include "logic_expressions.h"
#include "json-yaml.h"
#include <cleanup.h>

// FIX: remove
#include "syntax.h"

#include <assert.h>

int yylex(void);
extern char *yytext;

static int RelevantBundle(const char *agent, const char *blocktype);
static bool LvalWantsBody(char *stype, char *lval);
static SyntaxTypeMatch CheckSelection(const char *type, const char *name, const char *lval, Rval rval);
static SyntaxTypeMatch CheckConstraint(const char *type, const char *lval, Rval rval, const PromiseTypeSyntax *ss);
static void fatal_yyerror(const char *s);

static void ParseErrorColumnOffset(int column_offset, const char *s, ...) FUNC_ATTR_PRINTF(2, 3);
static void ParseError(const char *s, ...) FUNC_ATTR_PRINTF(1, 2);
static void ParseWarning(unsigned int warning, const char *s, ...) FUNC_ATTR_PRINTF(2, 3);

static void ValidateClassLiteral(const char *class_literal);

static bool INSTALL_SKIP = false;
static size_t CURRENT_BLOCKID_LINE = 0;
static size_t CURRENT_PROMISER_LINE = 0;

#define YYMALLOC xmalloc

#define ParserDebug(...) LogDebug(LOG_MOD_PARSER, __VA_ARGS__)

%}

%token IDSYNTAX BLOCKID QSTRING CLASS PROMISE_TYPE BUNDLE BODY ASSIGN ARROW NAKEDVAR
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
                           ParserDebug("P:bundle:%s\n", P.blocktype);
                           P.block = "bundle";
                           RvalDestroy(P.rval);
                           P.rval = RvalNew(NULL, RVAL_TYPE_NOPROMISEE);
                           RlistDestroy(P.currentRlist);
                           P.currentRlist = NULL;
                           if (P.currentstring)
                           {
                               free(P.currentstring);
                           }
                           P.currentstring = NULL;
                           strcpy(P.blockid,"");
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
                           ParserDebug("P:body:%s\n", P.blocktype);
                           P.block = "body";
                           strcpy(P.blockid,"");
                           RlistDestroy(P.currentRlist);
                           P.currentRlist = NULL;
                           if (P.currentstring)
                           {
                               free(P.currentstring);
                           }
                           P.currentstring = NULL;
                       }

bodytype_values:       typeid
                       {
                           if (!BodySyntaxGet(P.blocktype))
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
                           ParserDebug("P:%s:%s:%s arglist begin:%s\n", P.block,P.blocktype,P.blockid, yytext);
                       }

arglist_end:           ')'
                       {
                           ParserDebug("P:%s:%s:%s arglist end:%s\n", P.block,P.blocktype,P.blockid, yytext);
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

aitems:                aitem
                     | aitems ',' aitem

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

aitem:                 IDSYNTAX  /* recipient of argument is never a literal */
                       {
                           ParserDebug("P:%s:%s:%s  arg id: %s\n", P.block,P.blocktype,P.blockid, P.currentid);
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
                           if (RelevantBundle(CF_AGENTTYPES[P.agent_type], P.blocktype))
                           {
                               INSTALL_SKIP = false;
                           }
                           else if (strcmp(CF_AGENTTYPES[P.agent_type], P.blocktype) != 0)
                           {
                               INSTALL_SKIP = true;
                           }

                           if (!INSTALL_SKIP)
                           {
                               P.currentbundle = PolicyAppendBundle(P.policy, P.current_namespace, P.blockid, P.blocktype, P.useargs, P.filename);
                               P.currentbundle->offset.line = CURRENT_BLOCKID_LINE;
                               P.currentbundle->offset.start = P.offsets.last_block_id;
                           }
                           else
                           {
                               P.currentbundle = NULL;
                           }

                           RlistDestroy(P.useargs);
                           P.useargs = NULL;
                       }

                       bundle_decl

                       '}'
                       {
                           INSTALL_SKIP = false;
                           P.offsets.last_id = -1;
                           P.offsets.last_string = -1;
                           P.offsets.last_class_id = -1;

                           if (P.currentbundle)
                           {
                               P.currentbundle->offset.end = P.offsets.current;
                           }
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

body_begin:            '{'
                       {
                           ParserDebug("P:%s:%s:%s begin body open\n", P.block,P.blocktype,P.blockid);
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

bundle_statement:      promise_type classpromises_decl

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

promise_type:          PROMISE_TYPE             /* BUNDLE ONLY */
                       {
                           ParserDebug("\tP:%s:%s:%s promise_type = %s\n", P.block, P.blocktype, P.blockid, P.currenttype);

                           const PromiseTypeSyntax *promise_type_syntax = PromiseTypeSyntaxGet(P.blocktype, P.currenttype);

                           if (promise_type_syntax)
                           {
                               switch (promise_type_syntax->status)
                               {
                               case SYNTAX_STATUS_DEPRECATED:
                                   ParseWarning(PARSER_WARNING_DEPRECATED, "Deprecated promise type '%s' in bundle type '%s'", promise_type_syntax->promise_type, promise_type_syntax->bundle_type);
                                   // Intentional fall
                               case SYNTAX_STATUS_NORMAL:
                                   if (strcmp(P.block, "bundle") == 0)
                                   {
                                       if (!INSTALL_SKIP)
                                       {
                                           P.currentstype = BundleAppendPromiseType(P.currentbundle,P.currenttype);
                                           P.currentstype->offset.line = P.line_no;
                                           P.currentstype->offset.start = P.offsets.last_promise_type_id;
                                       }
                                       else
                                       {
                                           P.currentstype = NULL;
                                       }
                                   }
                                   break;
                               case SYNTAX_STATUS_REMOVED:
                                   ParseWarning(PARSER_WARNING_REMOVED, "Removed promise type '%s' in bundle type '%s'", promise_type_syntax->promise_type, promise_type_syntax->bundle_type);
                                   INSTALL_SKIP = true;
                                   break;
                               }
                           }
                           else
                           {
                               ParseError("Unknown promise type '%s'", P.currenttype);
                               INSTALL_SKIP = true;
                           }
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
                           ParserDebug("\tP:%s:%s:%s:%s:%s promiser = %s\n", P.block, P.blocktype, P.blockid, P.currenttype, P.currentclasses ? P.currentclasses : "any", P.promiser);
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
                           if (!INSTALL_SKIP)
                           {
                               const PromiseTypeSyntax *promise_type_syntax = PromiseTypeSyntaxGet(P.blocktype, P.currenttype);
                               assert(promise_type_syntax);

                               const ConstraintSyntax *constraint_syntax = PromiseTypeSyntaxGetConstraintSyntax(promise_type_syntax, P.lval);
                               if (constraint_syntax)
                               {
                                   switch (constraint_syntax->status)
                                   {
                                   case SYNTAX_STATUS_DEPRECATED:
                                       ParseWarning(PARSER_WARNING_DEPRECATED, "Deprecated constraint '%s' in promise type '%s'", constraint_syntax->lval, promise_type_syntax->promise_type);
                                       // Intentional fall
                                   case SYNTAX_STATUS_NORMAL:
                                       {
                                           const char *item = P.rval.item;
                                           // convert @(x) to mergedata(x)
                                           if (P.rval.type == RVAL_TYPE_SCALAR &&
                                               (strcmp(P.lval, "data") == 0 || strcmp(P.lval, "template_data") == 0) &&
                                               strlen(item) > 3 &&
                                               item[0] == '@' &&
                                               (item[1] == '(' || item[1] == '{'))
                                           {
                                               Rlist *synthetic_args = NULL;
                                               char *tmp = xstrndup(P.rval.item+2, strlen(P.rval.item)-3 );
                                               RlistAppendScalar(&synthetic_args, tmp);
                                               free(tmp);
                                               RvalDestroy(P.rval);

                                               P.rval = (Rval) { FnCallNew("mergedata", synthetic_args), RVAL_TYPE_FNCALL };
                                           }
                                           // convert 'json or yaml' to direct container or parsejson(x) or parseyaml(x)
                                           else if (P.rval.type == RVAL_TYPE_SCALAR &&
                                                    (strcmp(P.lval, "data") == 0 || strcmp(P.lval, "template_data") == 0))
                                           {
                                               JsonElement *json = NULL;
                                               JsonParseError res;
                                               bool json_parse_attempted = false;
                                               Buffer *copy = BufferNewFrom(P.rval.item, strlen(P.rval.item));

                                               const char* fname = NULL;
                                               if (strlen(P.rval.item) > 3 && strncmp("---", P.rval.item, 3) == 0)
                                               {
                                                   fname = "parseyaml";

                                                   // look for unexpanded variables
                                                   if (strstr(P.rval.item, "$(") == NULL &&
                                                       strstr(P.rval.item, "${") == NULL)
                                                   {
                                                       const char *copy_data = BufferData(copy);
                                                       res = JsonParseYamlString(&copy_data, &json);
                                                       json_parse_attempted = true;
                                                   }
                                               }
                                               else
                                               {
                                                   fname = "parsejson";
                                                   // look for unexpanded variables
                                                   if (strstr(P.rval.item, "$(") == NULL &&
                                                       strstr(P.rval.item, "${") == NULL)
                                                   {
                                                       const char *copy_data = BufferData(copy);
                                                       res = JsonParse(&copy_data, &json);
                                                       json_parse_attempted = true;
                                                   }
                                               }

                                               BufferDestroy(copy);

                                               if (json_parse_attempted && res != JSON_PARSE_OK)
                                               {
                                                   // Parsing failed, insert fncall so it can be retried during evaluation
                                               }
                                               else if (json != NULL &&
                                                        JsonGetElementType(json) == JSON_ELEMENT_TYPE_PRIMITIVE)
                                               {
                                                   // Parsing failed, insert fncall so it can be retried during evaluation
                                                   JsonDestroy(json);
                                                   json = NULL;
                                               }

                                               if (fname != NULL)
                                               {
                                                   if (json == NULL)
                                                   {
                                                       Rlist *synthetic_args = NULL;
                                                       RlistAppendScalar(&synthetic_args, xstrdup(P.rval.item));
                                                       RvalDestroy(P.rval);

                                                       P.rval = (Rval) { FnCallNew(xstrdup(fname), synthetic_args), RVAL_TYPE_FNCALL };
                                                   }
                                                   else
                                                   {
                                                       RvalDestroy(P.rval);
                                                       P.rval = (Rval) { json, RVAL_TYPE_CONTAINER };
                                                   }
                                               }
                                           }

                                           {
                                               SyntaxTypeMatch err = CheckConstraint(P.currenttype, P.lval, P.rval, promise_type_syntax);
                                               if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
                                               {
                                                   yyerror(SyntaxTypeMatchToString(err));
                                               }
                                           }

                                           if (P.rval.type == RVAL_TYPE_SCALAR && (strcmp(P.lval, "ifvarclass") == 0 || strcmp(P.lval, "if") == 0))
                                           {
                                               ValidateClassLiteral(P.rval.item);
                                           }

                                           Constraint *cp = PromiseAppendConstraint(P.currentpromise, P.lval, RvalCopy(P.rval), P.references_body);
                                           cp->offset.line = P.line_no;
                                           cp->offset.start = P.offsets.last_id;
                                           cp->offset.end = P.offsets.current;
                                           cp->offset.context = P.offsets.last_class_id;
                                           P.currentstype->offset.end = P.offsets.current;
                                       }
                                       break;
                                   case SYNTAX_STATUS_REMOVED:
                                       ParseWarning(PARSER_WARNING_REMOVED, "Removed constraint '%s' in promise type '%s'", constraint_syntax->lval, promise_type_syntax->promise_type);
                                       break;
                                   }
                               }
                               else
                               {
                                   ParseError("Unknown constraint '%s' in promise type '%s'", P.lval, promise_type_syntax->promise_type);
                               }

                               RvalDestroy(P.rval);
                               P.rval = RvalNew(NULL, RVAL_TYPE_NOPROMISEE);
                               strcpy(P.lval,"no lval");
                               RlistDestroy(P.currentRlist);
                               P.currentRlist = NULL;
                           }
                           else
                           {
                               RvalDestroy(P.rval);
                               P.rval = RvalNew(NULL, RVAL_TYPE_NOPROMISEE);
                           }
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

constraint_id:         IDSYNTAX                        /* BUNDLE ONLY */
                       {
                           ParserDebug("\tP:%s:%s:%s:%s:%s:%s attribute = %s\n", P.block, P.blocktype, P.blockid, P.currenttype, P.currentclasses ? P.currentclasses : "any", P.promiser, P.currentid);

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
                           const BodySyntax *body_syntax = BodySyntaxGet(P.blocktype);

                           if (body_syntax)
                           {
                               INSTALL_SKIP = false;

                               switch (body_syntax->status)
                               {
                               case SYNTAX_STATUS_DEPRECATED:
                                   ParseWarning(PARSER_WARNING_DEPRECATED, "Deprecated body '%s' of type '%s'", P.blockid, body_syntax->body_type);
                                   // intentional fall
                               case SYNTAX_STATUS_NORMAL:
                                   P.currentbody = PolicyAppendBody(P.policy, P.current_namespace, P.blockid, P.blocktype, P.useargs, P.filename);
                                   P.currentbody->offset.line = CURRENT_BLOCKID_LINE;
                                   P.currentbody->offset.start = P.offsets.last_block_id;
                                   break;

                               case SYNTAX_STATUS_REMOVED:
                                   ParseWarning(PARSER_WARNING_REMOVED, "Removed body '%s' of type '%s'", P.blockid, body_syntax->body_type);
                                   INSTALL_SKIP = true;
                                   break;
                               }
                           }
                           else
                           {
                               ParseError("Invalid body type '%s'", P.blocktype);
                               INSTALL_SKIP = true;
                           }

                           RlistDestroy(P.useargs);
                           P.useargs = NULL;

                           strcpy(P.currentid,"");
                       }

                       bodyattribs

                       '}'
                       {
                           P.offsets.last_id = -1;
                           P.offsets.last_string = -1;
                           P.offsets.last_class_id = -1;
                           if (P.currentbody)
                           {
                               P.currentbody->offset.end = P.offsets.current;
                           }
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

                           if (!INSTALL_SKIP)
                           {
                               const BodySyntax *body_syntax = BodySyntaxGet(P.blocktype);
                               assert(body_syntax);

                               const ConstraintSyntax *constraint_syntax = BodySyntaxGetConstraintSyntax(body_syntax->constraints, P.lval);
                               if (constraint_syntax)
                               {
                                   switch (constraint_syntax->status)
                                   {
                                   case SYNTAX_STATUS_DEPRECATED:
                                       ParseWarning(PARSER_WARNING_DEPRECATED, "Deprecated constraint '%s' in body type '%s'", constraint_syntax->lval, body_syntax->body_type);
                                       // Intentional fall
                                   case SYNTAX_STATUS_NORMAL:
                                       {
                                           SyntaxTypeMatch err = CheckSelection(P.blocktype, P.blockid, P.lval, P.rval);
                                           if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
                                           {
                                               yyerror(SyntaxTypeMatchToString(err));
                                           }

                                           if (P.rval.type == RVAL_TYPE_SCALAR && (strcmp(P.lval, "ifvarclass") == 0 || strcmp(P.lval, "if") == 0))
                                           {
                                               ValidateClassLiteral(P.rval.item);
                                           }

                                           Constraint *cp = NULL;
                                           if (P.currentclasses == NULL)
                                           {
                                               cp = BodyAppendConstraint(P.currentbody, P.lval, RvalCopy(P.rval), "any", P.references_body);
                                           }
                                           else
                                           {
                                               cp = BodyAppendConstraint(P.currentbody, P.lval, RvalCopy(P.rval), P.currentclasses, P.references_body);
                                           }

                                           if (P.currentvarclasses != NULL)
                                           {
                                               ParseError("Body attributes can't be put under a variable class '%s'", P.currentvarclasses);
                                           }

                                           cp->offset.line = P.line_no;
                                           cp->offset.start = P.offsets.last_id;
                                           cp->offset.end = P.offsets.current;
                                           cp->offset.context = P.offsets.last_class_id;
                                           break;
                                       }
                                   case SYNTAX_STATUS_REMOVED:
                                       ParseWarning(PARSER_WARNING_REMOVED, "Removed constraint '%s' in promise type '%s'", constraint_syntax->lval, body_syntax->body_type);
                                       break;
                                   }
                               }
                           }
                           else
                           {
                               RvalDestroy(P.rval);
                               P.rval = RvalNew(NULL, RVAL_TYPE_NOPROMISEE);
                           }

                           if (strcmp(P.blockid,"control") == 0 && strcmp(P.blocktype,"file") == 0)
                           {
                               if (strcmp(P.lval,"namespace") == 0)
                               {
                                   if (P.rval.type != RVAL_TYPE_SCALAR)
                                   {
                                       yyerror("namespace must be a constant scalar string");
                                   }
                                   else
                                   {
                                       free(P.current_namespace);
                                       P.current_namespace = xstrdup(P.rval.item);
                                   }
                               }
                           }

                           RvalDestroy(P.rval);
                           P.rval = RvalNew(NULL, RVAL_TYPE_NOPROMISEE);
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

selection_id:          IDSYNTAX
                       {
                           ParserDebug("\tP:%s:%s:%s:%s attribute = %s\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentid);

                           if (!INSTALL_SKIP)
                           {
                               const BodySyntax *body_syntax = BodySyntaxGet(P.currentbody->type);

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
                           ParserDebug("\tP:%s:%s:%s:%s %s = %s\n", P.block, P.blocktype, P.blockid, P.currenttype, P.currentclasses ? "class": "varclass", yytext);

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
                           ParserDebug("\tP:%s:%s:%s:%s id rval, %s = %s\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.lval, P.currentid);
                           RvalDestroy(P.rval);
                           P.rval = (Rval) { xstrdup(P.currentid), RVAL_TYPE_SCALAR };
                           P.references_body = true;
                       }
                     | BLOCKID
                       {
                           ParserDebug("\tP:%s:%s:%s:%s blockid rval, %s = %s\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.lval, P.currentid);
                           RvalDestroy(P.rval);
                           P.rval = (Rval) { xstrdup(P.currentid), RVAL_TYPE_SCALAR };
                           P.references_body = true;
                       }
                     | QSTRING
                       {
                           ParserDebug("\tP:%s:%s:%s:%s qstring rval, %s = %s\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.lval, P.currentstring);
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
                           ParserDebug("\tP:%s:%s:%s:%s nakedvar rval, %s = %s\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.lval, P.currentstring);
                           RvalDestroy(P.rval);
                           P.rval = (Rval) { P.currentstring, RVAL_TYPE_SCALAR };

                           P.currentstring = NULL;
                           P.references_body = false;
                       }
                     | list
                       {
                           ParserDebug("\tP:%s:%s:%s:%s install list =  %s\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.lval);
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
                                       P.block, P.blocktype, P.blockid,
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
                                       P.block, P.blocktype, P.blockid,
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
                           ParserDebug("\tP:%s:%s:%s:%s list append: nakedvar = %s\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentstring);
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
                           ParserDebug("\tP:%s:%s:%s:%s function id = %s\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentid);
                       }
                     | BLOCKID
                       {
                           ParserDebug("\tP:%s:%s:%s:%s function blockid = %s\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentid);
                       }
                     | NAKEDVAR
                       {
                           ParserDebug("\tP:%s:%s:%s:%s function nakedvar = %s\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentstring);
                           strncpy(P.currentid,P.currentstring,CF_MAXVARSIZE); // Make a var look like an ID
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

usefunction:           functionid givearglist
                       {
                           ParserDebug("\tP:%s:%s:%s:%s Finished with function, now at level %d\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.arg_nesting);
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

givearglist:           '('
                       {
                           if (++P.arg_nesting >= CF_MAX_NESTING)
                           {
                               fatal_yyerror("Nesting of functions is deeper than recommended");
                           }
                           P.currentfnid[P.arg_nesting] = xstrdup(P.currentid);
                           ParserDebug("\tP:%s:%s:%s begin givearglist for function %s, level %d\n", P.block,P.blocktype,P.blockid, P.currentfnid[P.arg_nesting], P.arg_nesting );
                       }

                       gaitems

                       ')'
                       {
                           ParserDebug("\tP:%s:%s:%s end givearglist for function %s, level %d\n", P.block,P.blocktype,P.blockid, P.currentfnid[P.arg_nesting], P.arg_nesting );
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
                           ParserDebug("\tP:%s:%s:%s:%s function %s, id arg = %s\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentfnid[P.arg_nesting], P.currentid);
                           /* currently inside a use function */
                           RlistAppendScalar(&P.giveargs[P.arg_nesting],P.currentid);
                       }

                     | QSTRING
                       {
                           /* currently inside a use function */
                           ParserDebug("\tP:%s:%s:%s:%s function %s, qstring arg = %s\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentfnid[P.arg_nesting], P.currentstring);
                           RlistAppendScalar(&P.giveargs[P.arg_nesting],P.currentstring);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | NAKEDVAR
                       {
                           /* currently inside a use function */
                           ParserDebug("\tP:%s:%s:%s:%s function %s, nakedvar arg = %s\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentfnid[P.arg_nesting], P.currentstring);
                           RlistAppendScalar(&P.giveargs[P.arg_nesting],P.currentstring);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | usefunction
                       {
                           /* Careful about recursion */
                           ParserDebug("\tP:%s:%s:%s:%s function %s, nakedvar arg = %s\n", P.block, P.blocktype, P.blockid, P.currentclasses ? P.currentclasses : "any", P.currentfnid[P.arg_nesting], P.currentstring);
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

/*****************************************************************/

static void ParseErrorVColumnOffset(int column_offset, const char *s, va_list ap)
{
    char *errmsg = StringVFormat(s, ap);
    fprintf(stderr, "%s:%d:%d: error: %s\n", P.filename, P.line_no, P.line_pos + column_offset, errmsg);
    free(errmsg);

    P.error_count++;

    /* Current line is not set when syntax error in first line */
    if (P.current_line)
    {
        fprintf(stderr, "%s\n", P.current_line);
        fprintf(stderr, "%*s\n", P.line_pos + column_offset, "^");

    }

    if (P.error_count > 12)
    {
        fprintf(stderr, "Too many errors\n");
        DoCleanupAndExit(EXIT_FAILURE);
    }

}

static void ParseErrorColumnOffset(int column_offset, const char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    ParseErrorVColumnOffset(column_offset, s, ap);
    va_end(ap);
}

static void ParseErrorV(const char *s, va_list ap)
{
    ParseErrorVColumnOffset(0, s, ap);
}

static void ParseError(const char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    ParseErrorV(s, ap);
    va_end(ap);
}

static void ParseWarningV(unsigned int warning, const char *s, va_list ap)
{
    if (((P.warnings | P.warnings_error) & warning) == 0)
    {
        return;
    }

    char *errmsg = StringVFormat(s, ap);
    const char *warning_str = ParserWarningToString(warning);

    fprintf(stderr, "%s:%d:%d: warning: %s [-W%s]\n", P.filename, P.line_no, P.line_pos, errmsg, warning_str);
    fprintf(stderr, "%s\n", P.current_line);
    fprintf(stderr, "%*s\n", P.line_pos, "^");

    free(errmsg);

    P.warning_count++;

    if ((P.warnings_error & warning) != 0)
    {
        P.error_count++;
    }

    if (P.error_count > 12)
    {
        fprintf(stderr, "Too many errors\n");
        DoCleanupAndExit(EXIT_FAILURE);
    }
}

static void ParseWarning(unsigned int warning, const char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    ParseWarningV(warning, s, ap);
    va_end(ap);
}

void yyerror(const char *str)
{
    ParseError("%s", str);
}

static void fatal_yyerror(const char *s)
{
    char *sp = yytext;
    /* Skip quotation mark */
    if (sp && *sp == '\"' && sp[1])
    {
        sp++;
    }

    fprintf(stderr, "%s: %d,%d: Fatal error during parsing: %s, near token \'%.20s\'\n", P.filename, P.line_no, P.line_pos, s, sp ? sp : "NULL");
    DoCleanupAndExit(EXIT_FAILURE);
}

static int RelevantBundle(const char *agent, const char *blocktype)
{
    if ((strcmp(agent, CF_AGENTTYPES[AGENT_TYPE_COMMON]) == 0) || (strcmp(CF_COMMONC, blocktype) == 0))
    {
        return true;
    }

/* Here are some additional bundle types handled by cfAgent */

    Item *ip = SplitString("edit_line,edit_xml", ',');

    if (strcmp(agent, CF_AGENTTYPES[AGENT_TYPE_AGENT]) == 0)
    {
        if (IsItemIn(ip, blocktype))
        {
            DeleteItemList(ip);
            return true;
        }
    }

    DeleteItemList(ip);
    return false;
}

static bool LvalWantsBody(char *stype, char *lval)
{
    for (int i = 0; i < CF3_MODULES; i++)
    {
        const PromiseTypeSyntax *promise_type_syntax = CF_ALL_PROMISE_TYPES[i];
        if (!promise_type_syntax)
        {
            continue;
        }

        for (int j = 0; promise_type_syntax[j].promise_type != NULL; j++)
        {
            const ConstraintSyntax *bs = promise_type_syntax[j].constraints;
            if (!bs)
            {
                continue;
            }

            if (strcmp(promise_type_syntax[j].promise_type, stype) != 0)
            {
                continue;
            }

            for (int l = 0; bs[l].lval != NULL; l++)
            {
                if (strcmp(bs[l].lval, lval) == 0)
                {
                    if (bs[l].dtype == CF_DATA_TYPE_BODY)
                    {
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }
            }
        }
    }

    return false;
}

static SyntaxTypeMatch CheckSelection(const char *type, const char *name, const char *lval, Rval rval)
{
    // Check internal control bodies etc
    if (strcmp("control", name) == 0)
    {
        for (int i = 0; CONTROL_BODIES[i].body_type != NULL; i++)
        {
            if (strcmp(type, CONTROL_BODIES[i].body_type) == 0)
            {
                const ConstraintSyntax *bs = CONTROL_BODIES[i].constraints;

                for (int l = 0; bs[l].lval != NULL; l++)
                {
                    if (strcmp(lval, bs[l].lval) == 0)
                    {
                        if (bs[l].dtype == CF_DATA_TYPE_BODY)
                        {
                            return SYNTAX_TYPE_MATCH_OK;
                        }
                        else if (bs[l].dtype == CF_DATA_TYPE_BUNDLE)
                        {
                            return SYNTAX_TYPE_MATCH_OK;
                        }
                        else
                        {
                            return CheckConstraintTypeMatch(lval, rval, bs[l].dtype, bs[l].range.validation_string, 0);
                        }
                    }
                }

            }
        }
    }

    // Now check the functional modules - extra level of indirection
    for (int i = 0; i < CF3_MODULES; i++)
    {
        const PromiseTypeSyntax *promise_type_syntax =  CF_ALL_PROMISE_TYPES[i];
        if (!promise_type_syntax)
        {
            continue;
        }

        for (int j = 0; promise_type_syntax[j].promise_type != NULL; j++)
        {
            const ConstraintSyntax *bs = bs = promise_type_syntax[j].constraints;

            if (!bs)
            {
                continue;
            }

            for (int l = 0; bs[l].lval != NULL; l++)
            {
                if (bs[l].dtype == CF_DATA_TYPE_BODY)
                {
                    const ConstraintSyntax *bs2 = bs[l].range.body_type_syntax->constraints;

                    if (bs2 == NULL || bs2 == (void *) CF_BUNDLE)
                    {
                        continue;
                    }

                    for (int k = 0; bs2[k].dtype != CF_DATA_TYPE_NONE; k++)
                    {
                        /* Either module defined or common */

                        if (strcmp(promise_type_syntax[j].promise_type, type) == 0 && strcmp(promise_type_syntax[j].promise_type, "*") != 0)
                        {
                            char output[CF_BUFSIZE];
                            snprintf(output, CF_BUFSIZE, "lval %s belongs to promise type '%s': but this is '%s'\n",
                                     lval, promise_type_syntax[j].promise_type, type);
                            yyerror(output);
                            return SYNTAX_TYPE_MATCH_OK;
                        }

                        if (strcmp(lval, bs2[k].lval) == 0)
                        {
                            /* Body definitions will be checked later. */
                            if (bs2[k].dtype != CF_DATA_TYPE_BODY)
                            {
                                return CheckConstraintTypeMatch(lval, rval, bs2[k].dtype, bs2[k].range.validation_string, 0);
                            }
                            else
                            {
                                return SYNTAX_TYPE_MATCH_OK;
                            }
                        }
                    }
                }
            }
        }
    }

    char output[CF_BUFSIZE];
    snprintf(output, CF_BUFSIZE, "Constraint lvalue \"%s\" is not allowed in \'%s\' constraint body", lval, type);
    yyerror(output);

    return SYNTAX_TYPE_MATCH_OK; // TODO: OK?
}

static SyntaxTypeMatch CheckConstraint(const char *type, const char *lval, Rval rval, const PromiseTypeSyntax *promise_type_syntax)
{
    assert(promise_type_syntax);

    if (promise_type_syntax->promise_type != NULL)     /* In a bundle */
    {
        if (strcmp(promise_type_syntax->promise_type, type) == 0)
        {
            const ConstraintSyntax *bs = promise_type_syntax->constraints;

            for (int l = 0; bs[l].lval != NULL; l++)
            {

                if (strcmp(lval, bs[l].lval) == 0)
                {
                    /* If we get here we have found the lval and it is valid
                       for this promise_type */

                    /* For bodies and bundles definitions can be elsewhere, so
                       they are checked in PolicyCheckRunnable(). */
                    if (bs[l].dtype != CF_DATA_TYPE_BODY &&
                        bs[l].dtype != CF_DATA_TYPE_BUNDLE)
                    {
                        return CheckConstraintTypeMatch(lval, rval, bs[l].dtype, bs[l].range.validation_string, 0);
                    }
                }
            }
        }
    }

    return SYNTAX_TYPE_MATCH_OK;
}

static void ValidateClassLiteral(const char *class_literal)
{
    ParseResult res = ParseExpression(class_literal, 0, strlen(class_literal));

    if (!res.result)
    {
        ParseErrorColumnOffset(res.position - strlen(class_literal), "Syntax error in context string");
    }

    FreeExpression(res.result);
}
