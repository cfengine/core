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
/* File: cfknow.c                                                            */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

enum cfknow_image
   {
   cf_no_image,
   cf_full_image,
   cf_impact_image,
   cf_special_quote
   };

int main (int argc,char *argv[]);
void CheckOpts(int argc,char **argv);
void ThisAgentInit(void);
void KeepKnowControlPromises(void);
void KeepKnowledgePromise(struct Promise *pp);
void VerifyTopicPromise(struct Promise *pp);
void VerifyOccurrencePromises(struct Promise *pp);
void GenerateSQL(void);
void LookupUniqueTopic(char *typed_topic);
void LookupMatchingTopics(char *typed_topic);
void ShowTopicDisambiguation(CfdbConn *cfdb,struct Topic *list,char *name);
void NothingFound(char *topic);
void ShowTopicCosmology(CfdbConn *cfdb,char *topic_name,char *topic_id,char *topic_type);
void ShowAssociationSummary(CfdbConn *cfdb,char *this_fassoc,char *this_tassoc);
void ShowAssociationCosmology(CfdbConn *cfdb,char *this_fassoc,char *this_tassoc,char *from_type,char *to_type);
char *Name2Id(char *s);
void ShowTextResults(char *name,char *type,struct Topic *other_topics,struct TopicAssociation *associations,struct Occurrence *occurrences,struct Topic *others);
void ShowHtmlResults(char *name,char *type,struct Topic *other_topics,struct TopicAssociation *associations,struct Occurrence *occurrences,struct Topic *others);
void ShowImageMap(char *this_name,char *this_type,struct Topic *other_topics,struct TopicAssociation *associations,struct Occurrence *occurrences,struct Topic *topics_this_type);
char *NextTopic(char *link,char* type);
char *NextMap(char *topic,char *type,enum cfknow_image imgtype);
void GenerateGraph(void);
void GenerateManual(void);
void VerifyOccurrenceGroup(char *file,struct Promise *pp);
void CfQueryCFDB(char *query);

/*******************************************************************/
/* GLOBAL VARIABLES                                                */
/*******************************************************************/

extern struct BodySyntax CFK_CONTROLBODY[];

enum typesequence
   {
   kp_classes,
   kp_topics,
   kp_occur,
   kp_reports,
   kp_none
   };

char *TYPESEQUENCE[] =
   {
   "classes",
   "topics",
   "occurrences",
   "reports",
   NULL
   };

char TM_PREFIX[CF_MAXVARSIZE];
char BUILD_DIR[CF_BUFSIZE];
char TOPIC_CMD[CF_MAXVARSIZE];

int HTML = false;
int WRITE_SQL = false;
int ISREGEX = false;
int SHOWMAP = cf_no_image;
int GRAPH = false;
int GENERATE_MANUAL = false;
char MANDIR[CF_BUFSIZE];
int PASS;

struct Occurrence *OCCURRENCES = NULL;

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

 char *ID = "The knowledge management agent is capable of building\n"
            "and analysing a semantic knowledge network. It can\n"
            "configure a relational database to contain an ISO\n"
            "standard topic map and permit regular-expression based\n"
            "searching of the map. Analysis of the semantic network\n"
            "can be performed providing graphical output of the data,\n"
            "and cf-know can assemble and converge the reference manual\n"
            "for the current version of the Cfengine software.";
 
 struct option OPTIONS[19] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "version",no_argument,0,'V' },
      { "file",required_argument,0,'f' },
      { "graphs",no_argument,0,'g'},
      { "html",no_argument,0,'H'},
      { "manual",no_argument,0,'m'},
      { "manpage",no_argument,0,'M'},
      { "map-full",required_argument,0,'K'},
      { "map-impact",required_argument,0,'k'},
      { "query_cfdb",required_argument,0,'Q'},
      { "quote",required_argument,0,'q'},
      { "regex",required_argument,0,'r'},
      { "sql",no_argument,0,'s'},
      { "syntax",required_argument,0,'S'},
      { "topic",required_argument,0,'t'},
      { NULL,0,0,'\0' }
      };

 char *HINTS[19] =
      {
      "Print the help message",
      "Set debugging level 0,1,2,3",
      "Output verbose information about the behaviour of the agent",
      "Output the version of the software",
      "Specify an alternative input file than the default",
      "Generate graphs from topic map data",
      "Output queries in HTML",
      "Generate reference manual from internal data",
      "Generate reference manpage from internal data",
      "Show full image map for argument",
      "Show impact map for argument",
      "Query the CFDB for testing, etc",
      "Quote encapsulated HTML output through the query engine",
      "Specify a regular expression for searching the topic map",
      "Store topic map in defined SQL database",
      "Print a syntax summary of the optional keyword or this cfengine version",
      "Specify a literal string topic to look up in the topic map",
      NULL
      };

/*****************************************************************************/

int main(int argc,char *argv[])

{
CheckOpts(argc,argv); 
GenericInitialize(argc,argv,"knowledge");
ThisAgentInit();
KeepKnowControlPromises();

if (strlen(TOPIC_CMD) == 0)
   {
   int complete;
   double percent;
   KeepPromiseBundles();
   GenerateGraph();
   GenerateSQL();
   GenerateManual();
   complete = (double)CF_NODES*(CF_NODES-1);
   percent = 100.0 * (double)CF_EDGES/(double)complete;
   CfOut(cf_inform,""," -> Complexity of knowledge model yields %d/%d = %.4lf%%\n",CF_EDGES,complete,percent);
   }
else
   {
   if (SHOWMAP == cf_special_quote)
      {
      CfHtmlHeader(stdout,TOPIC_CMD,STYLESHEET,WEBDRIVER,BANNER);
      SpecialQuote(TOPIC_CMD,"quoted");
      CfHtmlFooter(stdout,FOOTER);
      }
 else if (ISREGEX)
      {
      LookupMatchingTopics(TOPIC_CMD);
      }
   else
      {
      LookupUniqueTopic(TOPIC_CMD);
      }
   }

return 0;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  char arg[CF_BUFSIZE];
  struct Item *actionList;
  int optindex = 0;
  int c;
  char ld_library_path[CF_BUFSIZE];

strcpy(TOPIC_CMD,"");
LOOKUP = false;

while ((c=getopt_long(argc,argv,"ghHd:vVf:S:R:st:r:mMK:k:q:Q:",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

          if (optarg && strlen(optarg) < 5)
             {
             snprintf(arg,CF_MAXVARSIZE," -f used but argument \"%s\" incorrect",optarg);
             FatalError(arg);
             }

          strncpy(VINPUTFILE,optarg,CF_BUFSIZE-1);
          VINPUTFILE[CF_BUFSIZE-1] = '\0';
          MINUSF = true;
          break;

      case 'd': 

          switch ((optarg==NULL) ? '3' : *optarg)
             {
             case '1':
                 D1 = true;
                 DEBUG = true;
                 break;
             case '2':
                 D2 = true;
                 DEBUG = true;
                 break;
             default:
                 DEBUG = true;
                 break;
             }
          break;

      case 'g':
          GRAPH = true;
          break;

      case 'r':
          ISREGEX = true;
          LOOKUP = true;
          SHOWREPORTS = false;

      case 't':
          strcpy(TOPIC_CMD,optarg);
          LOOKUP = true;
          SHOWREPORTS = false;
          break;

      case 'K':
          strcpy(TOPIC_CMD,optarg);
          LOOKUP = true;
          SHOWREPORTS = false;
          SHOWMAP = 1;
          break;
          
      case 'k':
          strcpy(TOPIC_CMD,optarg);
          LOOKUP = true;
          SHOWREPORTS = false;
          SHOWMAP = 2;
          break;

      case 'q':
          strcpy(TOPIC_CMD,optarg);
          LOOKUP = true;
          SHOWREPORTS = false;
          SHOWMAP = 3;
          break;

      case 'Q':
/*
  { char buffer[CF_BUFSIZE];

  Nova_WebTopicMap_Initialize();
  Nova_PlotTopicCosmos(711,NULL,buffer,CF_BUFSIZE);
  printf("GOT %s\n",buffer);
  return;
  }
*/
          strcpy(TOPIC_CMD,optarg);
          CfQueryCFDB(TOPIC_CMD);
          exit(0);
          break;
          
      case 's':
          WRITE_SQL = true;
          break;
          
      case 'v':
          VERBOSE = true;
          break;
          
      case 'V':
          Version("cf-know");
          exit(0);
          
      case 'h':
          Syntax("cf-know - cfengine's knowledge agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'M':
          ManPage("cf-know - cfengine's knowledge agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'H':
          HTML = 1;
          break;

      case 'S':
          if (optarg)
             {
             SyntaxCompletion(optarg);
             exit(0);
             }
          break;

      case 'm':
          GENERATE_MANUAL = true;
          break;
          
      default: Syntax("cf-know - knowledge agent",OPTIONS,HINTS,ID);
          exit(1);
          
      }
  }

if (argv[optind] != NULL)
   {
   CfOut(cf_error,"","Unexpected argument with no preceding option: %s\n",argv[optind]);
   }

}

/*****************************************************************************/

void ThisAgentInit()

{ char vbuff[CF_BUFSIZE];
  int s1,s2;
 
strcpy(TM_PREFIX,"");
strcpy(WEBDRIVER,"");
strcpy(BANNER,"");
strcpy(FOOTER,"");
strcpy(STYLESHEET,"");
strcpy(BUILD_DIR,".");
strcpy(LICENSE_COMPANY,"");
strcpy(MANDIR,".");
strcpy(SQL_DATABASE,"cf_kmdb");
strcpy(SQL_OWNER,"");
strcpy(SQL_CONNECT_NAME,"");
strcpy(SQL_PASSWD,"");
strcpy(SQL_SERVER,"localhost");
strcpy(GRAPHDIR,"");
SHOWREPORTS = false;

#ifdef HAVE_LIBCFNOVA
s1 = Nova_SizeCfSQLContainer();
s2 = SizeCfSQLContainer();

if (s1 != s2)
   {
   CfOut(cf_error,""," !!! Pathological build. Nova module has different database combinations than core. %d (Nova) versus %d (Core)\n",s1,s2);
   FatalError("stop");
   }
#endif
}

/*****************************************************************************/

void KeepKnowControlPromises()

{ struct Constraint *cp;
  char rettype;
  void *retval;

for (cp = ControlBodyConstraints(cf_know); cp != NULL; cp=cp->next)
   {
   if (IsExcluded(cp->classes))
      {
      continue;
      }

   if (GetVariable("control_knowledge",cp->lval,&retval,&rettype) == cf_notype)
      {
      CfOut(cf_error,""," !! Unknown lval %s in knowledge control body",cp->lval);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_tm_prefix].lval) == 0)
      {
      strncpy(TM_PREFIX,retval,CF_MAXVARSIZE);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_builddir].lval) == 0)
      {
      strncpy(BUILD_DIR,retval,CF_BUFSIZE);

      if (strlen(MANDIR) < 2) /* MANDIR defaults to BUILDDIR */
         {
         strncpy(MANDIR,retval,CF_BUFSIZE);
         }
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_type].lval) == 0)
      {
      SQL_TYPE = Str2dbType(retval);
      WebCache("SQL_TYPE",retval);
      
      if (SQL_TYPE == cfd_notype)
         {
         CfOut(cf_error,""," !! Database type \"%s\" is not a supported database type",retval);
         }
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_database].lval) == 0)
      {
      strncpy(SQL_DATABASE,retval,CF_MAXVARSIZE);
      WebCache("SQL_DATABASE",retval);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_owner].lval) == 0)
      {
      strncpy(SQL_OWNER,retval,CF_MAXVARSIZE);
      WebCache("SQL_OWNER",retval);
      continue;
      }
      
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_passwd].lval) == 0)
      {
      strncpy(SQL_PASSWD,retval,CF_MAXVARSIZE);
      WebCache("SQL_PASSWD",retval);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_server].lval) == 0)
      {
      strncpy(SQL_SERVER,retval,CF_MAXVARSIZE);
      WebCache("SQL_SERVER",retval);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_connect_db].lval) == 0)
      {
      strncpy(SQL_CONNECT_NAME,retval,CF_MAXVARSIZE);
      WebCache("SQL_CONNECT_NAME",retval);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_query_engine].lval) == 0)
      {
      strncpy(WEBDRIVER,retval,CF_MAXVARSIZE);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_htmlbanner].lval) == 0)
      {
      strncpy(BANNER,retval,2*CF_BUFSIZE-1);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_htmlfooter].lval) == 0)
      {
      strncpy(FOOTER,retval,CF_BUFSIZE-1);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_stylesheet].lval) == 0)
      {
      strncpy(STYLESHEET,retval,CF_MAXVARSIZE);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_query_output].lval) == 0)
      {
      if (strcmp(retval,"html") == 0 || strcmp(retval,"HTML") == 0)
         {
         HTML = true;
         }
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_graph_output].lval) == 0)
      {
      GRAPH = GetBoolean(retval);
      CfOut(cf_verbose,"","SET graph_output = %d\n",GRAPH);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_views].lval) == 0)
      {
      VIEWS = GetBoolean(retval);
      CfOut(cf_verbose,"","SET view_projections = %d\n",VIEWS);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_graph_dir].lval) == 0)
      {
      strncpy(GRAPHDIR,retval,CF_MAXVARSIZE);
      CfOut(cf_verbose,"","SET graph_directory = %s\n",GRAPHDIR);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_genman].lval) == 0)
      {
      GENERATE_MANUAL = GetBoolean(retval);
      CfOut(cf_verbose,"","SET generate_manual = %d\n",GENERATE_MANUAL);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_mandir].lval) == 0)
      {
      strncpy(MANDIR,retval,CF_MAXVARSIZE);
      CfOut(cf_verbose,"","SET manual_source_directory = %s\n",MANDIR);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_docroot].lval) == 0)
      {
      strncpy(DOCROOT,retval,CF_MAXVARSIZE);
      CfOut(cf_verbose,"","SET document root = %s\n",DOCROOT);
      continue;
      }
   }
}

/*****************************************************************************/

void KeepPromiseBundles()
    
{ struct Bundle *bp;
  struct SubType *sp;
  struct Promise *pp;
  struct Rlist *rp,*params;
  struct FnCall *fp;
  char rettype,*name;
  void *retval;
  int ok = true;
  enum typesequence type;

if (GetVariable("control_common","bundlesequence",&retval,&rettype) == cf_notype)
   {
   CfOut(cf_error,""," !! No bundlesequence in the common control body");
   exit(1);
   }

for (rp = (struct Rlist *)retval; rp != NULL; rp=rp->next)
   {
   switch (rp->type)
      {
      case CF_SCALAR:
          name = (char *)rp->item;
          params = NULL;
          break;
      case CF_FNCALL:
          fp = (struct FnCall *)rp->item;
          name = (char *)fp->name;
          params = (struct Rlist *)fp->args;
          break;
          
      default:
          name = NULL;
          params = NULL;
          CfOut(cf_error,""," !! Illegal item found in bundlesequence: ");
          ShowRval(stdout,rp->item,rp->type);
          printf(" = %c\n",rp->type);
          ok = false;
          break;
      }
   
   if (!(GetBundle(name,"knowledge")||(GetBundle(name,"common"))))
      {
      CfOut(cf_error,""," !! Bundle \"%s\" listed in the bundlesequence was not found\n",name);
      ok = false;
      }
   }

if (!ok)
   {
   FatalError("Errors in knowledge bundles");
   }

/* If all is okay, go ahead and evaluate */

PASS = 1;

for (type = 0; TYPESEQUENCE[type] != NULL; type++)
   {
   for (rp = (struct Rlist *)retval; rp != NULL; rp=rp->next)
      {
      switch (rp->type)
         {
         case CF_FNCALL:
             fp = (struct FnCall *)rp->item;
             name = (char *)fp->name;
             params = (struct Rlist *)fp->args;
             break;
         default:
             name = (char *)rp->item;
             params = NULL;
             break;
         }
      
      if ((bp = GetBundle(name,"knowledge")) || (bp = GetBundle(name,"common")))
         {
         BannerBundle(bp,params);
         AugmentScope(bp->name,bp->args,params);
         DeletePrivateClassContext(); // Each time we change bundle      
         }
      
      if ((sp = GetSubTypeForBundle(TYPESEQUENCE[type],bp)) == NULL)
         {
         continue;      
         }
      
      BannerSubType(bp->name,sp->name,1);
      
      for (pp = sp->promiselist; pp != NULL; pp=pp->next)
         {
         ExpandPromise(cf_know,bp->name,pp,KeepKnowledgePromise);
         }      
      }
   }
}

/*********************************************************************/

void LookupUniqueTopic(char *classified_topic)

{ char query[CF_BUFSIZE],topic_name[CF_BUFSIZE],topic_id[CF_BUFSIZE],topic_context[CF_BUFSIZE],to_type[CF_BUFSIZE];
  char from_name[CF_BUFSIZE],to_name[CF_BUFSIZE],from_assoc[CF_BUFSIZE],to_assoc[CF_BUFSIZE];
  char cfrom_assoc[CF_BUFSIZE],cto_assoc[CF_BUFSIZE],ctopic_context[CF_BUFSIZE],cto_type[CF_BUFSIZE];
  char type[CF_MAXVARSIZE],topic[CF_MAXVARSIZE];
  int sql_database_defined = false,trymatch = false,count = 0,matched = 0;
  struct Topic *tp,*tmatches = NULL;
  char safe[CF_MAXVARSIZE],safetype[CF_MAXVARSIZE];
  CfdbConn cfdb;
  int s,e;


DeClassifyTopic(classified_topic,topic,type);

/* We need to set a scope for the regex stuff */

if (strlen(SQL_OWNER) > 0)
   {
   sql_database_defined = true;
   }
 
if (!sql_database_defined)
   {
   printf("No database defined...\n");
   return;
   }

CfConnectDB(&cfdb,SQL_TYPE,SQL_SERVER,SQL_OWNER,SQL_PASSWD,SQL_DATABASE);

if (!cfdb.connected)
   {
   CfOut(cf_error,""," !! Could not open sql_db %s\n",SQL_DATABASE);
   return;
   }

/* First assume the item is a topic */

CfOut(cf_verbose,""," -> Treating the search string %s as a literal string\n",topic);

strcpy(safe,EscapeSQL(&cfdb,topic));
strcpy(safetype,EscapeSQL(&cfdb,type));

if (strlen(type) > 0)
   {
   snprintf(query,CF_BUFSIZE,"SELECT topic_name,topic_id,topic_context,from topics where (topic_name='%s' or topic_id='%s') and topic_context='%s'",safe,CanonifyName(safe),safetype);
   }
else
   {
   snprintf(query,CF_BUFSIZE,"SELECT topic_name,topic_id,topic_context from topics where topic_name='%s' or topic_id='%s'",safe,CanonifyName(safe));
   }

CfNewQueryDB(&cfdb,query);

if (cfdb.maxcolumns != 3)
   {
   CfOut(cf_error,""," !! The topics database table did not promise the expected number of fields - got %d expected %d\n",cfdb.maxcolumns,3);
   CfCloseDB(&cfdb);
   return;
   }

while(CfFetchRow(&cfdb))
   {
   strncpy(topic_name,CfFetchColumn(&cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_id,CfFetchColumn(&cfdb,1),CF_BUFSIZE-1);
   strncpy(topic_context,CfFetchColumn(&cfdb,2),CF_BUFSIZE-1);
   AddTopic(&tmatches,topic_name,topic_context);   
   count++;
   }

CfDeleteQuery(&cfdb);

if (count >= 1)
   {
   ShowTopicCosmology(&cfdb,topic_name,topic_id,topic_context);
   CfCloseDB(&cfdb);
   return;
   }

/* This can only come about by illegal repetition in a direct lookup
if (count > 1)
   {
   ShowTopicDisambiguation(&cfdb,tmatches,topic);
   CfCloseDB(&cfdb);
   return;
   }
*/

/* If no matches, try the associations */

count = 0;
matched = 0;
tmatches = NULL;

strncpy(safe,EscapeSQL(&cfdb,topic),CF_MAXVARSIZE);

snprintf(query,CF_BUFSIZE,"SELECT from_name,from_type,from_assoc,to_assoc,to_type,to_name from associations where from_assoc='%s' or to_assoc='%s'",safe,safe);

/* Expect multiple matches always with associations */

CfNewQueryDB(&cfdb,query);

if (cfdb.maxcolumns != 6)
   {
   CfOut(cf_error,""," !! The associations database table did not promise the expected number of fields - got %d expected %d\n",cfdb.maxcolumns,6);
   CfCloseDB(&cfdb);
   return;
   }

while (CfFetchRow(&cfdb))
   {
   strncpy(from_name,CfFetchColumn(&cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_context,CfFetchColumn(&cfdb,1),CF_BUFSIZE-1);
   strncpy(from_assoc,CfFetchColumn(&cfdb,2),CF_BUFSIZE-1);
   strncpy(to_assoc,CfFetchColumn(&cfdb,3),CF_BUFSIZE-1);
   strncpy(to_type,CfFetchColumn(&cfdb,4),CF_BUFSIZE-1);
   strncpy(to_name,CfFetchColumn(&cfdb,5),CF_BUFSIZE-1);

   if (!AddTopic(&tmatches,from_assoc,to_assoc))
      {
      matched++;      
      strncpy(ctopic_context,topic_context,CF_BUFSIZE-1);
      strncpy(cfrom_assoc,from_assoc,CF_BUFSIZE-1);
      strncpy(cto_assoc,to_assoc,CF_BUFSIZE-1);
      strncpy(cto_type,to_type,CF_BUFSIZE-1);
      }

   count++;
   }

CfDeleteQuery(&cfdb);

if (matched == 0)
   {
   NothingFound(classified_topic);
   CfCloseDB(&cfdb);
   return;
   }

if (matched > 1)
   {
   /* End assoc summary */
   
   if (HTML)
      {
      CfHtmlHeader(stdout,"Matching associations",STYLESHEET,WEBDRIVER,BANNER);
      }
   
   for (tp = tmatches; tp != NULL; tp=tp->next)
      {
      ShowAssociationSummary(&cfdb,tp->topic_name,tp->topic_context);
      }

   if (HTML)
      {
      CfHtmlFooter(stdout,FOOTER);
      }
   
   CfCloseDB(&cfdb);
   return;
   }

if (matched == 1)
   {
   ShowAssociationCosmology(&cfdb,cfrom_assoc,cto_assoc,ctopic_context,cto_type);
   }

CfCloseDB(&cfdb);
}

/*********************************************************************/

void LookupMatchingTopics(char *classified_topic)

{ char query[CF_BUFSIZE],topic_name[CF_BUFSIZE],topic_id[CF_BUFSIZE],topic_context[CF_BUFSIZE],to_type[CF_BUFSIZE];
  char from_name[CF_BUFSIZE],to_name[CF_BUFSIZE],from_assoc[CF_BUFSIZE],to_assoc[CF_BUFSIZE];
  char cfrom_assoc[CF_BUFSIZE],cto_assoc[CF_BUFSIZE],ctopic_context[CF_BUFSIZE],cto_type[CF_BUFSIZE];
  char type[CF_MAXVARSIZE],topic[CF_MAXVARSIZE];
  int  sql_database_defined = false,trymatch = false,count = 0,matched = 0;
  struct Topic *tp,*tmatches = NULL;
  CfdbConn cfdb;
  int s,e;

DeClassifyTopic(classified_topic,topic,type);

CfOut(cf_verbose,""," -> Looking up topics matching \"%s\"\n",topic);

/* We need to set a scope for the regex stuff */

if (strlen(SQL_OWNER) > 0)
   {
   sql_database_defined = true;
   }
 
if (!sql_database_defined)
   {
   CfOut(cf_error,""," !! No knowlegde database defined...\n");
   return;
   }

CfConnectDB(&cfdb,SQL_TYPE,SQL_SERVER,SQL_OWNER,SQL_PASSWD,SQL_DATABASE);

if (!cfdb.connected)
   {
   CfOut(cf_error,""," !! Could not open sql_db %s\n",SQL_DATABASE);
   return;
   }

/* First assume the item is a topic */

snprintf(query,CF_BUFSIZE,"SELECT topic_name,topic_id,topic_context from topics");

CfNewQueryDB(&cfdb,query);

if (cfdb.maxcolumns != 3)
   {
   CfOut(cf_error,""," !! The topics database table did not promise the expected number of fields - got %d expected %d\n",cfdb.maxcolumns,3);
   CfCloseDB(&cfdb);
   return;
   }

while(CfFetchRow(&cfdb))
   {
   strncpy(topic_name,CfFetchColumn(&cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_id,CfFetchColumn(&cfdb,1),CF_BUFSIZE-1);
   strncpy(topic_context,CfFetchColumn(&cfdb,2),CF_BUFSIZE-1);

   if (BlockTextCaseMatch(topic,topic_name,&s,&e))
      {
      AddTopic(&tmatches,topic_name,topic_context);
      matched++;
      }
   }

CfDeleteQuery(&cfdb);

if (matched > 1)
   {
   ShowTopicDisambiguation(&cfdb,tmatches,topic);
   CfCloseDB(&cfdb);
   return;
   }

if (matched == 1)
   {
   for (tp = tmatches; tp != NULL; tp=tp->next)
      {
      ShowTopicCosmology(&cfdb,tp->topic_name,topic_id,tp->topic_context);
      }
   CfCloseDB(&cfdb);
   return;
   }

/* If no matches, try the associations */

count = 0;
matched = 0;
tmatches = NULL;

snprintf(query,CF_BUFSIZE,"SELECT from_name,from_type,from_assoc,to_assoc,to_type,to_name from associations");

/* Expect multiple matches always with associations */

CfNewQueryDB(&cfdb,query);

if (cfdb.maxcolumns != 6)
   {
   CfOut(cf_error,""," !! The associations database table did not promise the expected number of fields - got %d expected %d\n",cfdb.maxcolumns,6);
   CfCloseDB(&cfdb);
   return;
   }

while(CfFetchRow(&cfdb))
   {
   strncpy(from_name,CfFetchColumn(&cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_context,CfFetchColumn(&cfdb,1),CF_BUFSIZE-1);
   strncpy(from_assoc,CfFetchColumn(&cfdb,2),CF_BUFSIZE-1);
   strncpy(to_assoc,CfFetchColumn(&cfdb,3),CF_BUFSIZE-1);
   strncpy(to_type,CfFetchColumn(&cfdb,4),CF_BUFSIZE-1);
   strncpy(to_name,CfFetchColumn(&cfdb,5),CF_BUFSIZE-1);

   if (BlockTextCaseMatch(topic,from_assoc,&s,&e)||BlockTextCaseMatch(topic,to_assoc,&s,&e))
      {
      if (!AddTopic(&tmatches,from_assoc,to_assoc))
         {
         matched++;         
         strncpy(ctopic_context,topic_context,CF_BUFSIZE-1);
         strncpy(cfrom_assoc,from_assoc,CF_BUFSIZE-1);
         strncpy(cto_assoc,to_assoc,CF_BUFSIZE-1);
         strncpy(cto_type,to_type,CF_BUFSIZE-1);
         }
      }
   }

CfDeleteQuery(&cfdb);

if (matched == 0)
   {
   NothingFound(classified_topic);
   CfCloseDB(&cfdb);
   return;
   }

if (matched > 1)
   {
   /* End assoc summary */
   
   if (HTML)
      {
      CfHtmlHeader(stdout,"Matching associations",STYLESHEET,WEBDRIVER,BANNER);
      }
   
   for (tp = tmatches; tp != NULL; tp=tp->next)
      {
      ShowAssociationSummary(&cfdb,tp->topic_name,tp->topic_context);
      }

   if (HTML)
      {
      CfHtmlFooter(stdout,FOOTER);
      }
   
   CfCloseDB(&cfdb);
   return;
   }

if (matched == 1)
   {
   ShowAssociationCosmology(&cfdb,cfrom_assoc,cto_assoc,ctopic_context,cto_type);
   }

CfCloseDB(&cfdb);
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void KeepKnowledgePromise(struct Promise *pp)

{
if (pp->done)
   {
   return;
   }

if (strcmp("classes",pp->agentsubtype) == 0)
   {
   CfOut(cf_verbose,""," ! Class promises do not have any effect here.\n");
   return;
   }

if (strcmp("topics",pp->agentsubtype) == 0)
   {
   VerifyTopicPromise(pp);
   return;
   }

if (strcmp("occurrences",pp->agentsubtype) == 0)
   {
   VerifyOccurrencePromises(pp);
   return;
   }

if (strcmp("reports",pp->agentsubtype) == 0)
   {
   VerifyReportPromise(pp);
   return;
   }
}

/*********************************************************************/

void CfQueryCFDB(char *query)
{
#ifdef HAVE_LIBCFNOVA
Nova_CfQueryCFDB(query);
#endif
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void VerifyTopicPromise(struct Promise *pp)

{ char id[CF_BUFSIZE];
  char fwd[CF_BUFSIZE],bwd[CF_BUFSIZE];
  struct Attributes a = {0};
  struct Topic *tp = NULL,*tpn,*tp_sub;
  struct Rlist *rp,*rps,*contexts;
  char *handle = (char *)GetConstraint("handle",pp,CF_SCALAR);

a = GetTopicsAttributes(pp);

CfOut(cf_verbose,""," -> Attempting to install topic %s::%s \n",pp->classes,pp->promiser);

// Add a standard reserved word

contexts = SplitContextExpression(pp->classes,pp);

for (rp = contexts; rp != NULL; rp = rp->next)
   {
   if ((tp = InsertTopic(pp->promiser,rp->item)) == NULL)
      {
      return;
      }

   CfOut(cf_verbose,""," -> New topic promise for \"%s\" about context \"%s\"",pp->promiser,rp->item);
   
   if (a.fwd_name && a.bwd_name)
      {
      AddTopicAssociation(&(tp->associations),a.fwd_name,a.bwd_name,a.associates,true);
      }
   
// Handle all synonyms as associations later
   
   for (rps = a.synonyms; rps != NULL; rps=rps->next)
      {
      IdempPrependRScalar(&(tp->synonyms),rps->item,CF_SCALAR);
      IdempInsertTopic(rps->item);
      }
   
   if (handle)
      {
      IdempPrependRScalar(&(tp->synonyms),handle,CF_SCALAR);
      InsertTopic(handle,"handles");
      }
   
// Treat comments as occurrences of information.

   if (pp->ref)
      {
      struct Rlist *list = NULL;
      snprintf(id,CF_MAXVARSIZE,"%s.%s",pp->classes,CanonifyName(pp->promiser));
      PrependRScalar(&list,"description",CF_SCALAR);
      AddOccurrence(&OCCURRENCES,pp->ref,list,cfk_literal,id);
      DeleteRlist(list);
      }
   
   if (handle)
      {
      struct Rlist *list = NULL;
      PrependRScalar(&list,handle,CF_SCALAR);
      AddTopicAssociation(&(tp->associations),"is the promise of","stands for",list,true);
      DeleteRlist(list);
      list = NULL;
      snprintf(id,CF_MAXVARSIZE,"%s.%s",pp->classes,handle);
      PrependRScalar(&list,"description",CF_SCALAR);
      AddOccurrence(&OCCURRENCES,pp->ref,list,cfk_literal,id);
      DeleteRlist(list);
      }
   }

DeleteRlist(contexts);
}

/*********************************************************************/

void VerifyOccurrencePromises(struct Promise *pp)

{ struct Attributes a = {0};
  struct Topic *tp;
  char name[CF_BUFSIZE];
  enum representations rep_type;
  struct Rlist *contexts,*rp;
  int s,e;

a = GetOccurrenceAttributes(pp);

if (a.represents == NULL)
   {
   CfOut(cf_error,""," ! Occurrence \"%s\" (type) promises no topics");
   return;
   }

if (a.rep_type)
   {
   rep_type = String2Representation(a.rep_type);
   }
else
   {
   rep_type = cfk_url;
   }

contexts = SplitContextExpression(pp->classes,pp);

for (rp = contexts; rp != NULL; rp = rp->next)
   {
   CfOut(cf_verbose,""," -> New occurrence promise for \"%s\" about context \"%s\"",pp->promiser,rp->item);
   
   switch (rep_type)
      {
      case cfk_file:
          
          if (a.web_root == NULL || a.path_root == NULL)
             {
             CfOut(cf_error,""," !! File pattern but no complete url mapping path_root -> web_root");
             return;
             }
          
          strncpy(name,a.path_root,CF_BUFSIZE-1);
          
          if (!JoinPath(name,pp->promiser))
             {
             CfOut(cf_error,""," !! Unable to form pathname in search for local files");
             return;
             }

          // FIXME - this should pass rp->item instead of pp->classes if we want to keep this
          
          LocateFilePromiserGroup(name,pp,VerifyOccurrenceGroup);
          break;
          
      default:
          
          AddOccurrence(&OCCURRENCES,pp->promiser,a.represents,rep_type,rp->item);
          break;
      }
   }

DeleteRlist(contexts);
}

/*********************************************************************/

void GenerateSQL()

{ struct Topic *tp;
  struct TopicAssociation *ta;
  struct Occurrence *op;
  FILE *fout = stdout;
  char filename[CF_BUFSIZE],longname[CF_BUFSIZE],query[CF_BUFSIZE],safe[CF_BUFSIZE],safe2[CF_BUFSIZE];
  struct Rlist *columns = NULL,*rp;
  int i,sql_database_defined = false, slot;
  struct Promise *pp;
  struct Attributes a = {0};
  CfdbConn cfdb;

AddSlash(BUILD_DIR);
snprintf(filename,CF_BUFSIZE-1,"%stopic_map.sql",BUILD_DIR);

if (WRITE_SQL && strlen(SQL_OWNER) > 0)
   {
   sql_database_defined = true;
   }

/* Set the only option used by the database code */

snprintf(query,CF_MAXVARSIZE-1,"%s",SQL_DATABASE);
pp = NewPromise("databases",query);
memset(&a,0,sizeof(a));
a.transaction.action = cfa_fix;
a.database.operation = "create";
pp->conlist = NULL;

CfOut(cf_verbose,""," -> Writing %s\n",filename);

/* Open channels */

if ((fout = fopen(filename,"w")) == NULL)
   {
   CfOut(cf_verbose,"fopen"," !! Cannot write to %s\n",filename);
   fout = fopen("/dev/null","w");
   }

if (sql_database_defined)
   {
   CfConnectDB(&cfdb,SQL_TYPE,SQL_SERVER,SQL_OWNER,SQL_PASSWD,SQL_DATABASE);

   if (!cfdb.connected)
      {
      CfOut(cf_inform,""," !! Could not connect an existing database %s\n",SQL_DATABASE);
      CfConnectDB(&cfdb,SQL_TYPE,SQL_SERVER,SQL_OWNER,SQL_PASSWD,SQL_CONNECT_NAME);

      if (!cfdb.connected)
         {
         CfOut(cf_error,""," !! Could not connect to the sql_db server for %s\n",SQL_DATABASE);
         return;
         }
      
      if (!VerifyDatabasePromise(&cfdb,SQL_DATABASE,a,pp))
         {
         return;
         }
      
      CfOut(cf_verbose,""," -> Suceeded in creating database \"%s\"- now close and reopen to initialize\n",SQL_DATABASE);
      CfCloseDB(&cfdb);
      CfConnectDB(&cfdb,SQL_TYPE,SQL_SERVER,SQL_OWNER,SQL_PASSWD,SQL_DATABASE);      
      }

   CfOut(cf_verbose,""," -> Successfully connected to \"%s\"\n",SQL_DATABASE);
   }
else
   {
   CfOut(cf_verbose,""," -> Unable to connect to \"%s\"\n",SQL_DATABASE);
   cfdb.connected = false;
   }

/* Schema - very simple */

fprintf(fout,"# CREATE DATABASE cf_topic_map\n");
fprintf(fout,"# USE %s_topic_map\n",TM_PREFIX);
 
snprintf(query,CF_BUFSIZE-1,
        "CREATE TABLE topics"
        "("
        "pid integer PRIMARY KEY,"
        "topic_name varchar(256),"
        "topic_id varchar(256),"
        "topic_context varchar(256),"
        "topic_evc float(24)"
        ");\n"
        );

fprintf(fout,"%s",query);

AppendRScalar(&columns,"pid,int",CF_SCALAR);
AppendRScalar(&columns,"topic_name,varchar,256",CF_SCALAR);
AppendRScalar(&columns,"topic_id,varchar,256",CF_SCALAR);
AppendRScalar(&columns,"topic_context,varchar,256",CF_SCALAR);
AppendRScalar(&columns,"topic_evc,float,24",CF_SCALAR);

snprintf(query,CF_MAXVARSIZE-1,"%s.topics",SQL_DATABASE);

if (sql_database_defined)
   {
   CfVerifyTablePromise(&cfdb,query,columns,a,pp);
   }

DeleteRlist(columns);
columns = NULL;

snprintf(query,CF_BUFSIZE-1,
        "CREATE TABLE associations"
        "("
        "from_name varchar(256),"
        "from_context varchar(256),"
        "from_id int,"
        "from_assoc varchar(256),"
        "to_assoc varchar(256),"
        "to_context varchar(256),"
        "to_name varchar(256)"
        "to_id int,"
        ");\n"
        );

fprintf(fout,"%s",query);

AppendRScalar(&columns,"from_name,varchar,256",CF_SCALAR);
AppendRScalar(&columns,"from_context,varchar,256",CF_SCALAR);
AppendRScalar(&columns,"from_id,int",CF_SCALAR);
AppendRScalar(&columns,"from_assoc,varchar,256,",CF_SCALAR);
AppendRScalar(&columns,"to_assoc,varchar,256",CF_SCALAR);
AppendRScalar(&columns,"to_context,varchar,256",CF_SCALAR);
AppendRScalar(&columns,"to_name,varchar,256",CF_SCALAR);
AppendRScalar(&columns,"to_id,int",CF_SCALAR);

snprintf(query,CF_MAXVARSIZE-1,"%s.associations",SQL_DATABASE);

if (sql_database_defined)
   {
   CfVerifyTablePromise(&cfdb,query,columns,a,pp);
   }

DeleteRlist(columns);
columns = NULL;

snprintf(query,CF_BUFSIZE-1,
        "CREATE TABLE occurrences"
        "("
        "context varchar(256),"
        "locator varchar(1024),"
        "locator_type varchar(256),"
        "subtype varchar(256)"
        ");\n"
        );

fprintf(fout,"%s",query);

AppendRScalar(&columns,"context,varchar,256",CF_SCALAR);
AppendRScalar(&columns,"locator,varchar,1024",CF_SCALAR);
AppendRScalar(&columns,"locator_type,varchar,256",CF_SCALAR);
AppendRScalar(&columns,"subtype,varchar,256",CF_SCALAR);

snprintf(query,CF_MAXVARSIZE-1,"%s.occurrences",SQL_DATABASE);

if (sql_database_defined)
   {
   CfVerifyTablePromise(&cfdb,query,columns,a,pp);
   }

DeleteRlist(columns);
columns = NULL;

/* Delete existing data and recreate */

snprintf(query,CF_BUFSIZE-1,"delete from topics\n",TM_PREFIX);
fprintf(fout,"%s",query);
CfVoidQueryDB(&cfdb,query);
snprintf(query,CF_BUFSIZE-1,"delete from associations\n",TM_PREFIX);
fprintf(fout,"%s",query);
CfVoidQueryDB(&cfdb,query);
snprintf(query,CF_BUFSIZE-1,"delete from occurrences\n",TM_PREFIX);
fprintf(fout,"%s",query);
CfVoidQueryDB(&cfdb,query);

/* Class types */

for (slot = 0; slot < CF_HASHTABLESIZE; slot++)
   {
   for (tp = TOPICHASH[slot]; tp != NULL; tp=tp->next)
      {
      strncpy(safe,EscapeSQL(&cfdb,tp->topic_name),CF_BUFSIZE-1);
      
      snprintf(query,CF_BUFSIZE-1,"INSERT INTO topics (topic_name,topic_id,topic_context,pid) values ('%s','%s','%s','%d')\n",safe,Name2Id(tp->topic_name),tp->topic_context,tp->id);
      
      fprintf(fout,"%s",query);
      Debug(" -> Add topic %s\n",tp->topic_name);
      CfVoidQueryDB(&cfdb,query);
      
      // Associations
      
      for (ta = tp->associations; ta != NULL; ta=ta->next)
         {
         for (rp = ta->associates; rp != NULL; rp=rp->next)
            {
            char to_type[CF_MAXVARSIZE],to_topic[CF_MAXVARSIZE];
            char tcontext[CF_MAXVARSIZE],ttype[CF_MAXVARSIZE],ttopic[CF_MAXVARSIZE];
            int to_id = GetTopicPid(rp->item);
            
            DeClassifyTopic(rp->item,to_topic,to_type);
            strncpy(tcontext,EscapeSQL(&cfdb,tp->topic_context),CF_MAXVARSIZE-1);
            strncpy(ttype,EscapeSQL(&cfdb,to_type),CF_MAXVARSIZE-1);
            strncpy(ttopic,EscapeSQL(&cfdb,to_topic),CF_MAXVARSIZE-1);
            
            snprintf(query,CF_BUFSIZE-1,"INSERT INTO associations (from_name,to_name,from_assoc,to_assoc,from_context,to_context,from_id,to_id) values ('%s','%s','%s','%s','%s','%s','%d','%d');\n",safe,ttopic,ta->fwd_name,ta->bwd_name,tcontext,ttype,tp->id,to_id);
            
            fprintf(fout,"%s",query);
            CfVoidQueryDB(&cfdb,query);
            Debug(" -> Add association %s\n",ta->fwd_name);
            }
         }
      
      for (rp = tp->synonyms; rp != NULL; rp=rp->next)
         {
         char to_type[CF_MAXVARSIZE],to_topic[CF_MAXVARSIZE];
         int to_id = GetTopicPid(rp->item);
         
         DeClassifyTopic(rp->item,to_topic,to_type);
         
         snprintf(query,CF_BUFSIZE-1,"INSERT INTO associations (from_name,to_name,from_assoc,to_assoc,from_context,to_context,from_id,to_id) values ('%s','%s','%s','%s','%s','%s','%d','%d');\n",safe,EscapeSQL(&cfdb,to_topic),NOVA_SYNONYM,NOVA_SYNONYM,tp->topic_context,to_type,tp->id,to_id);
         
         fprintf(fout,"%s",query);
         CfVoidQueryDB(&cfdb,query);
         Debug(" -> Add association %s\n",ta->fwd_name);      
         }
      }
   }

for (op = OCCURRENCES; op != NULL; op=op->next)
   {
   for (rp = op->represents; rp != NULL; rp=rp->next)
      {
      char safeexpr[CF_BUFSIZE];
      strcpy(safeexpr,EscapeSQL(&cfdb,op->locator));
      
      snprintf(query,CF_BUFSIZE-1,"INSERT INTO occurrences (context,locator,locator_type,subtype) values ('%s','%s','%d','%s')\n",op->occurrence_context,safeexpr,op->rep_type,rp->item);
      fprintf(fout,"%s",query);
 CfVoidQueryDB(&cfdb,query);
      Debug(" -> Add occurrence of %s\n",tp->topic_name);
      }
   }


/* Close channels */

fclose(fout);
DeletePromise(pp);

if (sql_database_defined)
   {
   CfCloseDB(&cfdb);
   }
}

/*********************************************************************/

void GenerateManual()

{
if (GENERATE_MANUAL)
   {
   TexinfoManual(MANDIR);
   }
}

/*********************************************************************/

void GenerateGraph()
{
struct Rlist *semantics = NULL;

if (GRAPH)
   {
#ifdef HAVE_LIBCFNOVA
   VerifyGraph(NULL,NULL);
   if (VIEWS)
      { 
      PrependRScalar(&semantics,NOVA_GIVES,CF_SCALAR);
      PrependRScalar(&semantics,NOVA_USES,CF_SCALAR);
      PrependRScalar(&semantics,NOVA_IMPACTS,CF_SCALAR);
      PrependRScalar(&semantics,NOVA_ISIMPACTED,CF_SCALAR);
      PrependRScalar(&semantics,NOVA_BUNDLE_DATA,CF_SCALAR);
      PrependRScalar(&semantics,NOVA_BUNDLE_DATA_INV_B,CF_SCALAR);
      PrependRScalar(&semantics,NOVA_BUNDLE_DATA_INV_P,CF_SCALAR);
      VerifyGraph(semantics,"influence");
      }
#else
# ifdef HAVE_LIBGVC
   VerifyGraph(NULL,NULL);
# endif
#endif
   }

DeleteRlist(semantics);
}

/*********************************************************************/

void ShowTopicDisambiguation(CfdbConn *cfdb,struct Topic *tmatches,char *name)

{ char banner[CF_BUFSIZE];
  struct Topic *tp;
 
if (HTML)
   {
   snprintf(banner,CF_BUFSIZE,"Disambiguation %s",name);
   CfHtmlHeader(stdout,banner,STYLESHEET,WEBDRIVER,BANNER);
   printf("<div id=\"disambig\">");

   printf("<p>More than one topic matched your search parameters.</p>");

   printf("<ul>");
   
   for (tp = tmatches; tp != NULL; tp=tp->next)
      {
      printf("<li>Topic \"%s\" found ",NextTopic(tp->topic_name,tp->topic_context));
      printf("in the context of \"%s\"\n",NextTopic(tp->topic_context,""));
      }

   printf("</ul>");
   printf("</div>\n");
   CfHtmlFooter(stdout,FOOTER);
   }
else
   {
   for (tp = tmatches; tp != NULL; tp=tp->next)
      {
      printf("Topic \"%s\" found in the context of \"%s\"\n",tp->topic_name,tp->topic_context);
      }
   }
}

/*********************************************************************/

void NothingFound(char *name)

{ char banner[CF_BUFSIZE];
  struct Topic *tp;

if (HTML)
   {
   snprintf(banner,CF_BUFSIZE,"Nothing found for %s",name);
   CfHtmlHeader(stdout,banner,STYLESHEET,WEBDRIVER,BANNER);
   printf("<div id=\"intro\">");
   printf("<p>Your search expression %s does not seem to match any topics\n",name);
   printf("</div>\n");
   CfHtmlFooter(stdout,FOOTER);
   }
else
   {
   printf("Your search expression does not seem to match any topics\n");
   }
}

/*********************************************************************/

void ShowTopicCosmology(CfdbConn *cfdb,char *this_name,char *this_id,char *this_type)

{ struct Topic *other_topics = NULL,*topics_this_type = NULL;
  struct TopicAssociation *associations = NULL;
  struct Occurrence *occurrences = NULL;
  char topic_name[CF_BUFSIZE],topic_id[CF_BUFSIZE],topic_context[CF_BUFSIZE],associate[CF_BUFSIZE];
  char query[CF_BUFSIZE],fassociation[CF_BUFSIZE],bassociation[CF_BUFSIZE],safe[CF_BUFSIZE];
  char locator[CF_BUFSIZE],subtype[CF_BUFSIZE],to_type[CF_BUFSIZE];
  enum representations locator_type;
  struct Rlist *rp;

/* Collect data - first other topics of same type */

snprintf(query,CF_BUFSIZE,"SELECT topic_name,topic_id,topic_context from topics where topic_context='%s' order by topic_name desc",this_type);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 3)
   {
   CfOut(cf_error,""," !! The topics database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,3);
   return;
   }

while(CfFetchRow(cfdb))
   {
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_id,CfFetchColumn(cfdb,1),CF_BUFSIZE-1);
   strncpy(topic_context,CfFetchColumn(cfdb,2),CF_BUFSIZE-1);

   if (strcmp(topic_name,this_name) == 0 || strcmp(topic_id,this_name) == 0)
      {
      continue;
      }

   AddTopic(&other_topics,topic_name,topic_context);
   }

CfDeleteQuery(cfdb);

/* Collect data - then topics of this topic-type */

snprintf(query,CF_BUFSIZE,"SELECT topic_name,topic_id,topic_context from topics where topic_context='%s' order by topic_name desc",CanonifyName(this_name));

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 3)
   {
   CfOut(cf_error,""," !! The topics database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,3);
   return;
   }

while(CfFetchRow(cfdb))
   {
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_id,CfFetchColumn(cfdb,1),CF_BUFSIZE-1);
   strncpy(topic_context,CfFetchColumn(cfdb,2),CF_BUFSIZE-1);

   if (strcmp(topic_name,this_name) == 0 || strcmp(topic_id,this_name) == 0)
      {
      continue;
      }

   AddTopic(&topics_this_type,topic_name,topic_context);
   }

CfDeleteQuery(cfdb);

/* Then associated topics */

snprintf(query,CF_BUFSIZE,"SELECT from_name,from_type,from_assoc,to_assoc,to_type,to_name from associations where to_name='%s'",this_name);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 6)
   {
   CfOut(cf_error,""," !! The associations database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,6);
   return;
   }

/* Look in both directions for associations - first into */

while(CfFetchRow(cfdb))
   {
   struct Rlist *this = NULL;
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_context,CfFetchColumn(cfdb,1),CF_BUFSIZE-1);
   strncpy(fassociation,CfFetchColumn(cfdb,2),CF_BUFSIZE-1);
   strncpy(bassociation,CfFetchColumn(cfdb,3),CF_BUFSIZE-1);
   strncpy(to_type,CfFetchColumn(cfdb,4),CF_BUFSIZE-1);
   strncpy(associate,CfFetchColumn(cfdb,5),CF_BUFSIZE-1);

   AppendRlist(&this,ClassifiedTopic(topic_name,topic_context),CF_SCALAR);
   AddTopicAssociation(&associations,bassociation,NULL,this,false);
   DeleteRlist(this);
   }

CfDeleteQuery(cfdb);

/* ... then onto */

snprintf(query,CF_BUFSIZE,"SELECT from_name,from_type,from_assoc,to_assoc,to_type,to_name from associations where from_name='%s'",this_name);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 6)
   {
   CfOut(cf_error,""," !! The associations database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,6);
   return;
   }

while(CfFetchRow(cfdb))
   {
   struct Rlist *this = NULL;
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_context,CfFetchColumn(cfdb,1),CF_BUFSIZE-1);
   strncpy(fassociation,CfFetchColumn(cfdb,2),CF_BUFSIZE-1);
   strncpy(bassociation,CfFetchColumn(cfdb,3),CF_BUFSIZE-1);
   strncpy(to_type,CfFetchColumn(cfdb,4),CF_BUFSIZE-1);
   strncpy(associate,CfFetchColumn(cfdb,5),CF_BUFSIZE-1);

   AppendRlist(&this,ClassifiedTopic(associate,to_type),CF_SCALAR);
   AddTopicAssociation(&associations,fassociation,NULL,this,false);
   DeleteRlist(this);
   }

CfDeleteQuery(cfdb);

/* Finally occurrences of the mentioned topic */

strncpy(safe,EscapeSQL(cfdb,this_name),CF_BUFSIZE);
snprintf(query,CF_BUFSIZE,"SELECT topic_name,locator,locator_type,subtype,context from occurrences where topic_name='%s' or subtype='%s' order by locator_type,subtype",this_id,safe);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 5)
   {
   CfOut(cf_error,""," !! The occurrences database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,5);
   return;
   }

while(CfFetchRow(cfdb))
   {
   struct Rlist *this = NULL;
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_BUFSIZE-1);
   strncpy(locator,CfFetchColumn(cfdb,1),CF_BUFSIZE-1);
   locator_type = Str2Int(CfFetchColumn(cfdb,2));
   strncpy(subtype,CfFetchColumn(cfdb,3),CF_BUFSIZE-1);

   if (strcmp(subtype,this_name) == 0)
      {
      AppendRlist(&this,topic_name,CF_SCALAR);
      }
   else
      {
      AppendRlist(&this,subtype,CF_SCALAR);
      }

   strncpy(topic_context,CfFetchColumn(cfdb,4),CF_BUFSIZE-1);
   AddOccurrence(&occurrences,locator,this,locator_type,topic_context);
   DeleteRlist(this);
   }

CfDeleteQuery(cfdb);

/* Now print the results */

if (HTML)
   {
   if (SHOWMAP)
      {
      ShowImageMap(this_name,this_type,other_topics,associations,occurrences,topics_this_type);
      }
   else
      {
      ShowHtmlResults(this_name,this_type,other_topics,associations,occurrences,topics_this_type);
      }
   }
else
   {
   ShowTextResults(this_name,this_type,other_topics,associations,occurrences,topics_this_type);
   }
}

/*********************************************************************/

void ShowAssociationSummary(CfdbConn *cfdb,char *this_fassoc,char *this_tassoc)

{ char banner[CF_BUFSIZE];
 
if (HTML)
   {
   printf("<div id=\"intro\">");
   printf("<li>Association \"%s\" ",NextTopic(this_fassoc,""));
   printf("(inverse name \"%s\")\n",NextTopic(this_tassoc,""));
   printf("</div>");
   }
else
   {
   printf("Association \"%s\" (with inverse \"%s\"), ",this_fassoc,this_tassoc);
   }
}

/*********************************************************************/

void ShowAssociationCosmology(CfdbConn *cfdb,char *this_fassoc,char *this_tassoc,char *from_type,char *to_type)

{ char topic_name[CF_BUFSIZE],topic_id[CF_BUFSIZE],topic_context[CF_BUFSIZE],query[CF_BUFSIZE];
  char banner[CF_BUFSIZE],output[CF_BUFSIZE];
  char from_name[CF_BUFSIZE],to_name[CF_BUFSIZE],from_assoc[CF_BUFSIZE],to_assoc[CF_BUFSIZE];
  struct Rlist *flist = NULL,*tlist = NULL,*ftlist = NULL,*ttlist = NULL,*rp;
  int count = 0;
 

/* Role players - fwd search */

snprintf(query,CF_BUFSIZE,"SELECT from_name,from_type,from_assoc,to_assoc,to_type,to_name from associations where from_assoc='%s'",EscapeSQL(cfdb,this_fassoc));

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 6)
   {
   CfOut(cf_error,""," !! The associations database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,6);
   CfCloseDB(cfdb);
   return;
   }

while(CfFetchRow(cfdb))
   {
   strncpy(from_name,CfFetchColumn(cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_context,CfFetchColumn(cfdb,1),CF_BUFSIZE-1);
   strncpy(from_assoc,CfFetchColumn(cfdb,2),CF_BUFSIZE-1);
   strncpy(to_assoc,CfFetchColumn(cfdb,3),CF_BUFSIZE-1);
   strncpy(to_type,CfFetchColumn(cfdb,4),CF_BUFSIZE-1);
   strncpy(to_name,CfFetchColumn(cfdb,5),CF_BUFSIZE-1);

   if (HTML)
      {
      snprintf(output,CF_BUFSIZE,"<li>%s (%s)</li>\n",NextTopic(from_name,topic_context),topic_context);
      }
   else
      {
      snprintf(output,CF_BUFSIZE,"    %s (%s)\n",from_name,topic_context);
      }

   IdempPrependRScalar(&flist,output,CF_SCALAR);
   IdempPrependRScalar(&ftlist,topic_context,CF_SCALAR);

   if (HTML)
      {
      snprintf(output,CF_BUFSIZE,"<li>%s (%s)</li>\n",NextTopic(to_name,to_type),to_type);
      }
   else
      {
      snprintf(output,CF_BUFSIZE,"    %s (%s)\n",to_name,to_type);
      }

   IdempPrependRScalar(&tlist,output,CF_SCALAR);
   IdempPrependRScalar(&ttlist,to_type,CF_SCALAR);
   count++;
   }

CfDeleteQuery(cfdb);

/* Role players - inverse assoc search */

if (count == 0)
   {
   snprintf(query,CF_BUFSIZE,"SELECT from_name,from_type,from_assoc,to_assoc,to_type,to_name from associations where to_assoc='%s'",EscapeSQL(cfdb,this_tassoc));

   CfNewQueryDB(cfdb,query);
   
   if (cfdb->maxcolumns != 6)
      {
      CfOut(cf_error,""," !! The associations database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,6);
      CfCloseDB(cfdb);
      return;
      }
   
   while(CfFetchRow(cfdb))
      {
      strncpy(from_name,CfFetchColumn(cfdb,0),CF_BUFSIZE-1);
      strncpy(topic_context,CfFetchColumn(cfdb,1),CF_BUFSIZE-1);
      strncpy(from_assoc,CfFetchColumn(cfdb,2),CF_BUFSIZE-1);
      strncpy(to_assoc,CfFetchColumn(cfdb,3),CF_BUFSIZE-1);
      strncpy(to_type,CfFetchColumn(cfdb,4),CF_BUFSIZE-1);
      strncpy(to_name,CfFetchColumn(cfdb,5),CF_BUFSIZE-1);

      if (HTML)
         {
         snprintf(output,CF_BUFSIZE,"<li>%s (%s)</li>\n",NextTopic(from_name,topic_context),topic_context);
         }
      else
         {
         snprintf(output,CF_BUFSIZE,"    %s (%s)\n",from_name,from_type);
         }
      
      IdempPrependRScalar(&flist,output,CF_SCALAR);
      IdempPrependRScalar(&ftlist,topic_context,CF_SCALAR);
      
      if (HTML)
         {
         snprintf(output,CF_BUFSIZE,"<li>%s (%s)</li>\n",NextTopic(to_name,to_type),to_type);
         }
      else
         {
         snprintf(output,CF_BUFSIZE,"    %s (%s)\n",to_name,to_type);
         }
      
      IdempPrependRScalar(&tlist,output,CF_SCALAR);
      IdempPrependRScalar(&ttlist,to_type,CF_SCALAR);
      }

   CfDeleteQuery(cfdb);
   }


if (HTML)
   {
   snprintf(banner,CF_BUFSIZE,"A: \"%s\" (with inverse \"%s\")",this_fassoc,this_tassoc);
   CfHtmlHeader(stdout,banner,STYLESHEET,WEBDRIVER,BANNER);
   printf("<div id=\"intro\">");
   printf("\"%s\" associates topics of types: { ",this_fassoc);
   for (rp = ftlist; rp != NULL; rp=rp->next)
      {
      printf("\"%s\"",NextTopic(rp->item,""));
      if (rp->next)
         {
         printf(",");
         }
      }
   printf(" } &rarr; { \n");

   for (rp = ttlist; rp != NULL; rp=rp->next)
      {
      printf("\"%s\"",NextTopic(rp->item,""));
      if (rp->next)
         {
         printf(",");
         }
      }
   
   printf(" }");
   printf("</div>");
   }
else
   {
   printf("Association \"%s\" (with inverse \"%s\"), ",this_fassoc,this_tassoc);

   printf("\"%s\" associates topics of types: { ",this_fassoc);
   for (rp = ftlist; rp != NULL; rp=rp->next)
      {
      printf("\"%s\"",rp->item);
      if (rp->next)
         {
         printf(",");
         }
      }
   printf(" } &rarr; { \n");

   for (rp = ttlist; rp != NULL; rp=rp->next)
      {
      printf("\"%s\"",rp->item);
      if (rp->next)
         {
         printf(",");
         }
      }
   
   printf(" }");
   }

if (HTML)
   {
   printf("<p><div id=\"occurrences\">");
   printf("\n<h2>Role players</h2>\n\n");
   printf("  <h3>Origin::</h3>\n");
   printf("<div id=\"roles\">\n");
   printf("<ul>\n");
   }
else
   {
   printf("\nRole players\n\n");
   printf("  Origin::\n");
   }

for (rp = AlphaSortRListNames(flist); rp != NULL; rp=rp->next)
   {
   printf("%s\n",rp->item);
   }

if (HTML)
   {
   printf("</ul></div><p>");
   printf("  <h3>Associates::</h3>\n");
   printf("<div id=\"roles\">\n");
   printf("<ul>\n");
   }
else
   {
   printf("\n\nRole players\n\n");
   printf("  Associates::\n");
   }

for (rp = AlphaSortRListNames(tlist); rp != NULL; rp=rp->next)
   {
   printf("%s\n",rp->item);
   }

DeleteRlist(flist);
DeleteRlist(tlist);
DeleteRlist(ftlist);
DeleteRlist(ttlist);

if (HTML)
   {
   printf("</ul>\n</div></div>");
   CfHtmlFooter(stdout,FOOTER);
   }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

char *Name2Id(char *s)

{ static char ret[CF_BUFSIZE],detox[CF_BUFSIZE];
  char *sp;

strncpy(ret,s,CF_BUFSIZE-1);

for (sp = ret; *sp != '\0'; sp++)
   {
   if (!isalnum(*sp) && *sp != '_')
      {
      if (isalnum(*sp & 127))
         {
         *sp &= 127;
         }
      }
   }
  
strncpy(detox,CanonifyName(ret),CF_BUFSIZE-1);

if (TM_PREFIX && strlen(TM_PREFIX) > 0)
   {
   snprintf(ret,CF_BUFSIZE,"%s_%s",TM_PREFIX,detox);
   }
else
   {
   snprintf(ret,CF_BUFSIZE,"%s",detox);
   }
 
return ret;
}

/*********************************************************************/

void ShowTextResults(char *this_name,char *this_type,struct Topic *other_topics,struct TopicAssociation *associations,struct Occurrence *occurrences, struct Topic *topics_this_type)

{ struct Topic *tp;
  struct TopicAssociation *ta;
  struct Occurrence *oc;
  struct Rlist *rp;
  int count = 0;

printf("\nTopic \"%s\" found in the context of \"%s\"\n",this_name,this_type);

printf("\nResults:\n\n");

for (oc = occurrences; oc != NULL; oc=oc->next)
   {
   if (oc->represents == NULL)
      {
      printf("(direct)");
      }
   else
      {
      for (rp = oc->represents; rp != NULL; rp=rp->next)
         {
         printf("   %s: ",(char *)rp->item);
         }
      }
   switch (oc->rep_type)
      {
      case cfk_url:
      case cfk_web:
          printf(" %s (URL)\n",oc->locator);
          break;
      case cfk_file:
          printf("%s (file)\n",oc->locator);
          break;
      case cfk_db:
          printf(" %s (DB)\n",oc->locator);
          break;          
      case cfk_literal:
          printf(" \"%s\" (Text)\n",oc->locator);
          break;
      case cfk_image:
          printf("(Image)\n",oc->locator);
          break;
      default:
          break;
      }

   count++;
   }

if (count == 0)
   {
   printf("\n    (none)\n");
   }

count = 0;

printf("\n\nTopics of the type %s:\n\n",this_name);

for (tp = topics_this_type; tp != NULL; tp=tp->next)
   {
   printf("  %s\n",tp->topic_name);
   count++;
   }

if (count == 0)
   {
   printf("\n    (none)\n");
   }

printf("\nAssociations:\n\n");

for (ta = associations; ta != NULL; ta=ta->next)
   {
   printf("  %s \"%s\"\n",this_name,ta->fwd_name);
   
   for (rp = ta->associates; rp != NULL; rp=rp->next)
      {
      printf("    - %s\n",rp->item);
      }

   count++;
   }

if (count == 0)
   {
   printf("\n    (none)\n");
   }

count = 0;

printf("\nOther topics of the same type (%s):\n\n",this_type);

for (tp = other_topics; tp != NULL; tp=tp->next)
   {
   printf("  %s\n",tp->topic_name);
   count++;
   }

if (count == 0)
   {
   printf("\n    (none)\n");
   }
}

/*********************************************************************/

void ShowHtmlResults(char *this_name,char *this_type,struct Topic *other_topics,struct TopicAssociation *associations,struct Occurrence *occurrences,struct Topic *topics_this_type)

{ struct Topic *tp;
  struct TopicAssociation *ta;
  struct Occurrence *oc;
  struct Rlist *rp;
  struct stat sb;
  int count = 0, subcount = 0;
  FILE *fout = stdout;
  char banner[CF_BUFSIZE],filename[CF_BUFSIZE],pngfile[CF_BUFSIZE];
  char *v,rettype;
  void *retval;

if (GetVariable("control_common","version",&retval,&rettype) != cf_notype)
   {
   v = (char *)retval;
   }
else
   {
   v = "not specified";
   }

snprintf(banner,CF_BUFSIZE,"%s",ClassifiedTopic(this_name,this_type));  
CfHtmlHeader(stdout,banner,STYLESHEET,WEBDRIVER,BANNER);

/* Images */

fprintf(fout,"<div id=\"image\">");

snprintf(pngfile,CF_BUFSIZE,"graphs/%s.png",CanonifyName(ClassifiedTopic(this_name,this_type)));

if (cfstat(pngfile,&sb) != -1)
   {
   fprintf(fout,"<div id=\"tribe\"><a href=\"%s\"><img src=\"%s\"></a></div>",NextMap(this_name,this_type,cf_full_image),pngfile);
   }
else
   {
   if (cfstat(pngfile,&sb) != -1)
      {
      fprintf(fout,"<div id=\"tribe\"><a href=\"%s\"><img src=\"%s\"></a></div>",pngfile,pngfile);
      }   
   }

snprintf(pngfile,CF_BUFSIZE,"graphs/influence_%s.png",CanonifyName(ClassifiedTopic(this_name,this_type)));

if (cfstat(pngfile,&sb) != -1)
   {
   fprintf(fout,"<div id=\"influence\"><a href=\"%s\"><img src=\"%s\"></a></div>",NextMap(this_name,this_type,cf_impact_image),pngfile);
   }

fprintf(fout,"</div>\n");

/* div id=content_container

fprintf(fout,"<div id=\"intro\">");
fprintf(fout,"This topic \"%s\" has type ",NextTopic(this_name,this_type));
fprintf(fout,"\"%s\" in map version %s</div>\n",NextTopic(this_type,""),v);
*/

// Occurrences

if (occurrences != NULL)
   {
   char embed_link[CF_BUFSIZE];

   count = 0;
   
   fprintf(fout,"<p><div id=\"occurrences\">");
   
   fprintf(fout,"\n<h2>References to '<span id=\"subject\">%s</span>' in section `<span id=\"category\">%s</span>'</h2>\n\n",this_name,this_type);
   
   fprintf(fout,"<ul>\n");
   
   for (oc = occurrences; oc != NULL; oc=oc->next)
      {
      embed_link[0] = '\0';

      if (oc->represents == NULL)
         {
         fprintf(fout,"<li>(directly)");
         }
      else
         {
         fprintf(fout,"<li>");
	 
         for (rp = oc->represents; rp != NULL; rp=rp->next)
            {
            if (strlen(embed_link) == 0 && (strncmp(rp->item,"http",4) == 0 || *(char *)(rp->item) == '/'))
               {
               strcpy(embed_link,rp->item);
               continue;
               }
            else
               {
               if (rp != oc->represents)
                  {
                  fprintf(fout,",");
                  }
               fprintf(fout,"%s ",rp->item);
               }            
            }
         
         fprintf(fout,":");
         }
      
      switch (oc->rep_type)
         {
         case cfk_url:
             fprintf(fout,"<span id=\"url\"><a href=\"%s\">%s</a> </span>(URL)",URLControl(WEBDRIVER,oc->locator),oc->locator);
             break;
         case cfk_web:
             fprintf(fout,"<span id=\"url\"><a href=\"%s\"> ...%s</a> </span>(URL)",URLControl(WEBDRIVER,oc->locator),URLHint(oc->locator));
             break;
         case cfk_file:
             fprintf(fout," <a href=\"file://%s\">%s</a> (file)",URLControl(WEBDRIVER,oc->locator),oc->locator);
             break;
         case cfk_db:
             fprintf(fout," %s (DB)",URLControl(WEBDRIVER,oc->locator));
             break;          
         case cfk_literal:
             fprintf(fout,"<p> \"%s\" (Text)</p>",oc->locator);
             break;
         case cfk_image:
             if (strlen(embed_link)> 0)
                {
                fprintf(fout,"<p><div id=\"embedded_image\"><a href=\"%s\"><img src=\"%s\"></a></div></p>",URLControl(WEBDRIVER,embed_link),oc->locator);
                }
             else
                {
                fprintf(fout,"<p><div id=\"embedded_image\"><a href=\"%s\"><img src=\"%s\" border=\"0\"></a></div></p>",URLControl(WEBDRIVER,oc->locator),oc->locator);
                }
             break;
         case cfk_portal:
             fprintf(fout,"<p><a href=\"%s\" target=\"_blank\">%s</a> </span>(URL)",URLControl(WEBDRIVER,oc->locator),oc->locator);
             break;

         default:
             break;
         }
      
      count++;
      }
   
   fprintf(fout,"</ul>\n");
   
   if (count == 0)
      {
      fprintf(fout,"\n    (none)\n");
      }
   
   fprintf(fout,"</div>");
   }
else
   {
   fprintf(fout,"<p><div id=\"occurrences\">");
   fprintf(fout,"<i>No specific documents on this topic. Try following the leads and categories below.</i>");
   fprintf(fout,"</div>");
   }

// Associations

if (associations)
   {
   count = 0;
   
   fprintf(fout,"<p><div id=\"associations\">");
   fprintf(fout,"\n<h2>Insight, leads and perspectives:</h2>\n\n");
   
   fprintf(fout,"<ul>\n");
   
   for (ta = associations; ta != NULL; ta=ta->next)
      {
      fprintf(fout,"<li>  %s \"%s\"\n",this_name,NextTopic(ta->fwd_name,""));
      
      fprintf(fout,"<ul>\n");
      for (rp = ta->associates; rp != NULL; rp=rp->next)
         {
         fprintf(fout,"<li> %s\n",NextTopic(rp->item,""));
         }
      fprintf(fout,"</ul>\n");
      count++;
      }
   
   if (count == 0)
      {
      printf("<li>    (none)\n");
      }
   
   fprintf(fout,"</ul>\n");
   fprintf(fout,"</div>");
   }


// Tree-view

if (other_topics || topics_this_type)
   {
   count = 0;
   
   fprintf(fout,"<p><div id=\"others\">\n");
   
   fprintf(fout,"\n<h2>The rest of the category \"%s\" :</h2>\n\n",this_type);
   
   fprintf(fout,"<ul>\n");

   fprintf(fout,"<li>  %s\n",this_name);

   // Pseudo hierarchy

   // Enter this sub-topic

   count = 0;
   
   fprintf(fout,"<ul>\n");
   
   for (tp = topics_this_type; tp != NULL; tp=tp->next)
      {
      fprintf(fout,"<li>  %s\n",NextTopic(tp->topic_name,tp->topic_context));
      count++;
      }
   
   if (count == 0)
      {
      fprintf(fout,"<li>    (no sub-topics)\n");
      }
   
   fprintf(fout,"</ul>\n");
   
   // Back up a level
   
   for (tp = other_topics; tp != NULL; tp=tp->next)
      {
      fprintf(fout,"<li>  %s\n",NextTopic(tp->topic_name,tp->topic_context));
      subcount++;
      }
   
   if (subcount == 0)
      {
      fprintf(fout,"<li>    (no other topics in this category)\n");
      }
   
   fprintf(fout,"</div>");
   
   fprintf(fout,"</ul>\n");
   }

// end container

CfHtmlFooter(stdout,FOOTER);
}

/*********************************************************************/

void ShowImageMap(char *this_name,char *this_type,struct Topic *other_topics,struct TopicAssociation *associations,struct Occurrence *occurrences,struct Topic *topics_this_type)

{ FILE *fout = stdout, *fin;
  char banner[CF_BUFSIZE],filename[CF_BUFSIZE],pngfile[CF_BUFSIZE];
  char *v,rettype;
  void *retval;
  
if (GetVariable("control_common","version",&retval,&rettype) != cf_notype)
   {
   v = (char *)retval;
   }
else
   {
   v = "not specified";
   }

snprintf(banner,CF_BUFSIZE,"%s",ClassifiedTopic(this_name,this_type));  
CfHtmlHeader(fout,banner,STYLESHEET,WEBDRIVER,BANNER);

/* Images */

switch (SHOWMAP)
   {
   case cf_full_image:
       fprintf(fout,"<div id=\"fullimage\">\n");
       snprintf(filename,CF_BUFSIZE,"graphs/%s.map",CanonifyName(ClassifiedTopic(this_name,this_type)));
       break;
   case cf_impact_image:
       fprintf(fout,"<div id=\"impactimage\">\n");
       snprintf(filename,CF_BUFSIZE,"graphs/influence_%s.map",CanonifyName(ClassifiedTopic(this_name,this_type)));
       break;
   case cf_special_quote:
       SpecialQuote(this_name,this_type);
       CfHtmlFooter(fout,FOOTER);
       return;
   default:
       printf("<h1>No directive promised</h1>");
       CfHtmlFooter(fout,FOOTER);
       return;
   }

snprintf(pngfile,CF_BUFSIZE,"graphs/%s.png",CanonifyName(ClassifiedTopic(this_name,this_type)));

if ((fin = fopen(filename,"r")) == NULL)
   {
   fprintf(fout,"Unable to open the map fragment %s",filename);
   }
else
   {
   char line[CF_BUFSIZE],buffer[CF_BUFSIZE];

   while (!feof(fin))
      {
      line[0] = '\0';
      fgets(line,CF_BUFSIZE,fin);

      if (IsHtmlHeader(line))
         {
         continue;
         }
      
      snprintf(buffer,CF_BUFSIZE-1,line,WEBDRIVER);
      fprintf(fout,"%s",buffer);
      }
   
   fclose(fin);
   }


switch (SHOWMAP)
   {
   case cf_full_image:
   case cf_impact_image:
       fprintf(fout,"</div>\n");
       break;
   }

CfHtmlFooter(fout,FOOTER);
}

/*********************************************************************/

void VerifyOccurrenceGroup(char *file,struct Promise *pp)
    
{ struct Attributes a = {0};
  struct Topic *tp;
  enum representations rep_type;
  struct stat sb;
  char *sp,url[CF_BUFSIZE];
  struct Rval retval;

a = GetOccurrenceAttributes(pp);

if (a.rep_type)
   {
   rep_type = String2Representation(a.rep_type);
   }
else
   {
   rep_type = cfk_url;
   }

if (cfstat(file,&sb) == -1)
   {
   CfOut(cf_verbose,""," !! File %s matched but could not be read",file);
   return;
   }

if (a.path_root == NULL || a.web_root == NULL)
   {
   CfOut(cf_error,""," !! No pathroot/webroot defined in representation");
   PromiseRef(cf_error,pp);
   return;
   }

Chop(a.path_root);
DeleteSlash(a.path_root);
sp = file + strlen(a.path_root) + 1;

FullTextMatch(pp->promiser,sp);
retval = ExpandPrivateRval("this",a.represents,CF_LIST);
DeleteScope("match");

if (strlen(a.web_root) > 0)
   {
   snprintf(url,CF_BUFSIZE-1,"%s/%s",a.web_root,sp);
   }
else
   {
   snprintf(url,CF_BUFSIZE-1,"%s",sp);
   }

AddOccurrence(&OCCURRENCES,url,retval.item,cfk_url,pp->classes);
CfOut(cf_verbose,""," -> File %s matched and being logged at %s",file,url);

DeleteRlist((struct Rlist *)retval.item);
}

/*********************************************************************/
/* Referrals                                                         */
/*********************************************************************/

char *NextTopic(char *topic,char *type)

{ static char url[CF_BUFSIZE];
  char ctopic[CF_MAXVARSIZE],ctype[CF_MAXVARSIZE];
  
if (strlen(WEBDRIVER) == 0)
   {
   CfOut(cf_error,""," !! No query_engine is defined\n");
   exit(1);
   }

if (strchr(topic,':'))
   {
   DeClassifyTopic(topic,ctopic,ctype);
   if (ctype && strlen(ctype) > 0)
      {
      snprintf(url,CF_BUFSIZE,"<a href=\"%s?next=%s\">%s</a> (in %s)",WEBDRIVER,topic,ctopic,ctype);
      }
   else
      {
      snprintf(url,CF_BUFSIZE,"<a href=\"%s?next=%s::%s\">%s</a>",WEBDRIVER,type,topic,topic);
      }
   }
else
   {
   snprintf(url,CF_BUFSIZE,"<a href=\"%s?next=%s::%s\">%s</a>",WEBDRIVER,type,topic,topic);
   }

return url;
}

/*********************************************************************/

char *NextMap(char *topic,char *type,enum cfknow_image imgtype)

{ static char url[CF_BUFSIZE];
  char ctopic[CF_MAXVARSIZE],ctype[CF_MAXVARSIZE];
  char *webtype;

switch(imgtype)
   {
   case cf_special_quote:
       webtype = "quote";
       break;
   case cf_impact_image:
       webtype = "map2";
       break;
   default:
       webtype = "map1";
       break;
   }

if (strlen(WEBDRIVER) == 0)
   {
   CfOut(cf_error,""," !! No query_engine is defined\n");
   exit(1);
   }

if (strchr(topic,':'))
   {
   DeClassifyTopic(topic,ctopic,ctype);

   if (ctype && strlen(ctype) > 0)
      {
      snprintf(url,CF_BUFSIZE,"%s?%s=%s",WEBDRIVER,webtype,topic);
      }
   else
      {
      snprintf(url,CF_BUFSIZE,"%s?%s=%s::%s",WEBDRIVER,webtype,type,topic);
      }
   }
else
   {
   snprintf(url,CF_BUFSIZE,"%s?%s=%s::%s",WEBDRIVER,webtype,type,topic);
   }

return url;
}


/* EOF */




