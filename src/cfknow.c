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
/* File: cfknow.c                                                            */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

int main (int argc,char *argv[]);
void CheckOpts(int argc,char **argv);
void ThisAgentInit(void);
void KeepKnowControlPromises(void);
void KeepKnowledgePromise(struct Promise *pp);
void VerifyTopicPromise(struct Promise *pp);
void VerifyOccurrencePromises(struct Promise *pp);
void VerifyOntology(void);
void ShowOntology(void);
void ShowTopicMapLTM(void);
void ShowTopicLTM(FILE *fout,char *id,char *type,char *value);
void ShowAssociationsLTM(FILE *fout,char *id,char *type,struct TopicAssociation *ta);
void ShowOccurrencesLTM(FILE *fout,char *id,struct Occurrence *op);
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
char *NextTopic(char *link,char* type);
void GenerateGraph(void);
void GenerateManual(void);

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
char SQL_DATABASE[CF_MAXVARSIZE];
char SQL_OWNER[CF_MAXVARSIZE];
char SQL_PASSWD[CF_MAXVARSIZE];
char SQL_SERVER[CF_MAXVARSIZE];
char TOPIC_CMD[CF_MAXVARSIZE];
char WEBDRIVER[CF_MAXVARSIZE];
char BANNER[2*CF_BUFSIZE];
char FOOTER[CF_BUFSIZE];
char STYLESHEET[CF_MAXVARSIZE];
enum cfdbtype SQL_TYPE = cfd_notype;
int HTML = false;
int WRITE_SQL = false;
int ISREGEX = false;
int GRAPH = false;
int GENERATE_MANUAL = false;
char GRAPHDIR[CF_BUFSIZE];
char MANDIR[CF_BUFSIZE];

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

 char *ID = "The knowledge management agent is capable of building\n"
            "an analysing a semantic knowledge network. It can\n"
            "configure a relational database to contain an ISO\n"
            "standard topic map and permit regular-expression based\n"
            "searching of the map. Analysis of the semantic network\n"
            "can be performed providing graphical output of the data,\n"
            "and cfknow can assemble and converge the reference manual\n"
            "for the current version of the cfengine software.";
 
 struct option OPTIONS[14] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "dry-run",no_argument,0,'n' },
      { "version",no_argument,0,'V' },
      { "file",required_argument,0,'f' },
      { "graphs",no_argument,0,'g'},
      { "html",no_argument,0,'H'},
      { "manual",no_argument,0,'m'},
      { "regex",required_argument,0,'r'},
      { "sql",no_argument,0,'s'},
      { "syntax",no_argument,0,'S'},
      { "topic",required_argument,0,'t'},
      { NULL,0,0,'\0' }
      };

 char *HINTS[14] =
      {
      "Print the help message",
      "Set debugging level 0,1,2,3",
      "Output verbose information about the behaviour of the agent",
      "All talk and no action mode - make no changes, only inform of promises not kept",
      "Output the version of the software",
      "Specify an alternative input file than the default",
      "Generate graphs from topic map data",
      "Output queries in HTML",
      "Generate reference manual from internal data",
      "Specify a regular expression for searching the topic map",
      "Store topic map in defined SQL database",
      "Print an HTML syntax summary of this cfengine version",
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
   KeepPromiseBundles();
   VerifyOntology();
   ShowOntology(); // all types and assocs
   ShowTopicMapLTM(); // all types and assocs
   GenerateSQL();
   GenerateManual();
   GenerateGraph();
   }
else
   {
   if (ISREGEX)
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
  struct Item *actionList;
  int optindex = 0;
  int c;
  char ld_library_path[CF_BUFSIZE];

strcpy(TOPIC_CMD,"");

while ((c=getopt_long(argc,argv,"ghHd:vVf:Sst:r:mM",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

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

      case 't':
          strcpy(TOPIC_CMD,optarg);
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

      case 'S': SyntaxTree();
          exit(0);

      case 'm':
          GENERATE_MANUAL = true;
          break;
          
      default: Syntax("cf-know - knowledge agent",OPTIONS,HINTS,ID);
          exit(1);
          
      }
  }
}

/*****************************************************************************/

void ThisAgentInit()

{ char vbuff[CF_BUFSIZE];

strcpy(TM_PREFIX,"");
strcpy(WEBDRIVER,"");
strcpy(BANNER,"");
strcpy(FOOTER,"");
strcpy(STYLESHEET,"");
strcpy(BUILD_DIR,".");
strcpy(MANDIR,".");
strcpy(SQL_DATABASE,"cf_topic_map");
strcpy(SQL_OWNER,"");
strcpy(SQL_PASSWD,"");
strcpy(SQL_SERVER,"localhost");
strcpy(GRAPHDIR,"");
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
      CfOut(cf_error,"","Unknown lval %s in knowledge control body",cp->lval);
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
      if (strcmp(retval,"mysql") == 0)
         {
         SQL_TYPE = cfd_mysql;
         }
      
      if (strcmp(retval,"postgress") == 0)
         {
         SQL_TYPE = cfd_postgress;
         }
      
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_database].lval) == 0)
      {
      strncpy(SQL_DATABASE,retval,CF_MAXVARSIZE);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_owner].lval) == 0)
      {
      strncpy(SQL_OWNER,retval,CF_MAXVARSIZE);
      continue;
      }
      
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_passwd].lval) == 0)
      {
      strncpy(SQL_PASSWD,retval,CF_MAXVARSIZE);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_server].lval) == 0)
      {
      strncpy(SQL_SERVER,retval,CF_MAXVARSIZE);
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
   CfOut(cf_error,"","No bundlesequence in the common control body");
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
          CfOut(cf_error,"","Illegal item found in bundlesequence: ");
          ShowRval(stdout,rp->item,rp->type);
          printf(" = %c\n",rp->type);
          ok = false;
          break;
      }
   
   if (!(GetBundle(name,"knowledge")||(GetBundle(name,"common"))))
      {
      CfOut(cf_error,"","Bundle \"%s\" listed in the bundlesequence was not found\n",name);
      ok = false;
      }
   }

if (!ok)
   {
   FatalError("Errors in knowledge bundles");
   }

/* If all is okay, go ahead and evaluate */

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
             
   for (type = 0; TYPESEQUENCE[type] != NULL; type++)
      {
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

void VerifyOntology()

{ struct TopicAssociation *ta;
  struct Topic *tp;
  struct Rlist *rp;
  
for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {  
   for (ta = tp->associations; ta != NULL; ta=ta->next)
      { 
      for (rp = ta->associates; rp != NULL; rp=rp->next)
         {
         char *reverse_type = GetTopicType(TOPIC_MAP,rp->item);
         
         if (reverse_type == NULL)
            {
            CfOut(cf_inform,"","Topic \"%s\" is not (yet) declared with a type",rp->item);
            continue;
            }
         
         /* First populator sets the reverse type others must conform to */
         
         if (ta->associate_topic_type == NULL)
            {
            if ((ta->associate_topic_type = strdup(CanonifyName(reverse_type))) == NULL)
               {
               CfOut(cf_error,"malloc","Memory failure in AddTopicAssociation");
               FatalError("");
               }
            }
         
         if (ta->associate_topic_type && strcmp(ta->associate_topic_type,reverse_type) != 0)
            {
            CfOut(cf_inform,"","Associates in the relationship \"%s\" do not all have the same topic type",ta->fwd_name);
            CfOut(cf_inform,"","Found \"%s\" but expected \"%s\"",ta->associate_topic_type,reverse_type);
            }
         }
      }
   }
}

/*********************************************************************/

void ShowOntology()

{ struct Topic *tp;
  struct TopicAssociation *ta;
  FILE *fout = stdout;
  char filename[CF_BUFSIZE],longname[CF_BUFSIZE];
  struct Item *generic_associations = NULL;
  struct Item *generic_topics = NULL;
  struct Item *generic_types = NULL;
  struct Item *ip;

AddSlash(BUILD_DIR);
snprintf(filename,CF_BUFSIZE-1,"%sontology.html",BUILD_DIR);

CfOut(cf_verbose,"","Writing %s\n",filename);

if ((fout = fopen(filename,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","Cannot write to %s\n",filename);
   return;
   }

for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {
   for (ta = tp->associations; ta != NULL; ta=ta->next)
      {
      snprintf(longname,CF_BUFSIZE,"%s/%s",NextTopic(ta->fwd_name,""),ta->bwd_name);
      
      if (!IsItemIn(generic_associations,longname))
         {
         PrependItemList(&generic_associations,longname);            
         }
      }

   if (tp->comment)
      {
      snprintf(longname,CF_BUFSIZE,"%s (%s)",tp->comment,NextTopic(tp->topic_name,tp->topic_type));
      }
   else
      {
      snprintf(longname,CF_BUFSIZE,"%s",NextTopic(tp->topic_name,tp->topic_type));
      }

   if (!IsItemIn(generic_topics,longname))
      {
      PrependItemList(&generic_topics,longname);            
      }

   if (!IsItemIn(generic_types,NextTopic(tp->topic_type,"")))
      {
      PrependItemList(&generic_types,NextTopic(tp->topic_type,""));            
      }
   }

CfHtmlHeader(fout,"CfKnowledge Operational Ontology",STYLESHEET,WEBDRIVER,BANNER);

fprintf(fout,"<h2>Types</h2><p>\n");

ip = SortItemListNames(generic_types);
generic_types = ip;

ip = SortItemListNames(generic_associations);
generic_associations = ip;

ip = SortItemListNames(generic_topics);
generic_topics = ip;

/* Class types */

fprintf(fout,"<table>\n");

for (ip = generic_types; ip != NULL; ip=ip->next)
   {
   fprintf(fout,"<tr><td>%s</td></tr>\n",ip->name);
   }

fprintf(fout,"</table>\n");

/* Associations */

fprintf(fout,"<h2>Associations</h2><p>\n");

fprintf(fout,"<table>\n");

for (ip = generic_associations; ip != NULL; ip=ip->next)
   {
   fprintf(fout,"<tr><td>%s</td></tr>\n",ip->name);
   }

fprintf(fout,"</table>\n");

/* Topics & their types */

fprintf(fout,"<h2>Topics</h2><p>\n");

fprintf(fout,"<table>\n");

for (ip = generic_topics; ip != NULL; ip=ip->next)
   {
   fprintf(fout,"<tr><td>%s</td></tr>\n",ip->name);
   }

fprintf(fout,"</table>\n");

CfHtmlFooter(fout,FOOTER);
fclose(fout);
}

/*********************************************************************/

void ShowTopicMapLTM()

{ struct Topic *tp;
  struct TopicAssociation *ta;
  struct Occurrence *op;
  FILE *fout = stdout;
  char id[CF_MAXVARSIZE],filename[CF_BUFSIZE],longname[CF_BUFSIZE];
  struct Item *generic_associations = NULL;

AddSlash(BUILD_DIR);
snprintf(filename,CF_BUFSIZE-1,"%stopic_map.ltm",BUILD_DIR);

CfOut(cf_verbose,"","Writing %s\n",filename);

if ((fout = fopen(filename,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","Cannot write to %s\n",filename);
   return;
   }

fprintf(fout,"/*********************/\n");
fprintf(fout,"/* Class types       */\n");
fprintf(fout,"/*********************/\n\n");
   
for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {
   strncpy(id,Name2Id(tp->topic_name),CF_MAXVARSIZE-1);

   if (tp->comment)
      {
      snprintf(longname,CF_BUFSIZE,"%s (%s)",tp->comment,tp->topic_name);
      ShowTopicLTM(fout,id,tp->topic_type,longname);
      }
   else
      {
      ShowTopicLTM(fout,id,tp->topic_type,tp->topic_name);
      }

   for (ta = tp->associations; ta != NULL; ta=ta->next)
      {
      if (!IsItemIn(generic_associations,ta->fwd_name))
         {
         ShowAssociationsLTM(fout,NULL,NULL,ta);
         PrependItemList(&generic_associations,ta->fwd_name);            
         }
      }
   }

/* Second pass for details */

fprintf(fout,"\n/*********************/\n");
fprintf(fout,"/* Association types */\n");
fprintf(fout,"/*********************/\n\n");

for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {
   for (ta = tp->associations; ta != NULL; ta=ta->next)
      {
      ShowAssociationsLTM(fout,tp->topic_name,tp->topic_type,ta);
      }
   }

/* Occurrences */

fprintf(fout,"\n/*********************/\n");
fprintf(fout,"/* Occurrences       */\n");
fprintf(fout,"/*********************/\n\n");

for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {
   strncpy(id,CanonifyName(tp->topic_name),CF_MAXVARSIZE-1);
      
   for (op = tp->occurrences; op != NULL; op=op->next)
      {
      ShowOccurrencesLTM(fout,id,op);
      }
   }

fclose(fout);
}

/*********************************************************************/

void LookupUniqueTopic(char *typed_topic)

{ char query[CF_BUFSIZE],topic_name[CF_BUFSIZE],topic_id[CF_BUFSIZE],topic_type[CF_BUFSIZE],to_type[CF_BUFSIZE];
  char from_name[CF_BUFSIZE],to_name[CF_BUFSIZE],from_assoc[CF_BUFSIZE],to_assoc[CF_BUFSIZE];
  char cfrom_assoc[CF_BUFSIZE],cto_assoc[CF_BUFSIZE],ctopic_type[CF_BUFSIZE],cto_type[CF_BUFSIZE];
  char type[CF_MAXVARSIZE],topic[CF_MAXVARSIZE];
  int sql_database_defined = false,trymatch = false,count = 0,matched = 0;
  struct Topic *tp,*tmatches = NULL;
  char safe[CF_MAXVARSIZE],safetype[CF_MAXVARSIZE];
  CfdbConn cfdb;
  int s,e;


DeTypeTopic(typed_topic,topic,type);

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
   CfOut(cf_error,"","Could not open sql_db %s\n",SQL_DATABASE);
   return;
   }

/* First assume the item is a topic */

CfOut(cf_verbose,"","Treating the search string %s as a literal string\n",topic);

strcpy(safe,EscapeSQL(&cfdb,topic));
strcpy(safetype,EscapeSQL(&cfdb,type));

if (strlen(type) > 0)
   {
   snprintf(query,CF_BUFSIZE,"SELECT * from topics where (topic_name='%s' or topic_id='%s') and topic_type='%s'",safe,CanonifyName(safe),safetype);
   }
else
   {
   snprintf(query,CF_BUFSIZE,"SELECT * from topics where topic_name='%s' or topic_id='%s'",safe,CanonifyName(safe));
   }

CfNewQueryDB(&cfdb,query);

if (cfdb.maxcolumns != 3)
   {
   CfOut(cf_error,"","The topics database table did not promise the expected number of fields - got %d expected %d\n",cfdb.maxcolumns,3);
   CfCloseDB(&cfdb);
   return;
   }

while(CfFetchRow(&cfdb))
   {
   strncpy(topic_name,CfFetchColumn(&cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_id,CfFetchColumn(&cfdb,1),CF_BUFSIZE-1);
   strncpy(topic_type,CfFetchColumn(&cfdb,2),CF_BUFSIZE-1);
   AddTopic(&tmatches,topic_name,topic_type);   
   count++;
   }

CfDeleteQuery(&cfdb);

if (count == 1)
   {
   ShowTopicCosmology(&cfdb,topic_name,topic_id,topic_type);
   CfCloseDB(&cfdb);
   return;
   }

if (count > 1)
   {
   ShowTopicDisambiguation(&cfdb,tmatches,topic);
   CfCloseDB(&cfdb);
   return;
   }

/* If no matches, try the associations */

count = 0;
matched = 0;
tmatches = NULL;

strncpy(safe,EscapeSQL(&cfdb,topic),CF_MAXVARSIZE);

snprintf(query,CF_BUFSIZE,"SELECT * from associations where from_assoc='%s' or to_assoc='%s'",safe,safe);

/* Expect multiple matches always with associations */

CfNewQueryDB(&cfdb,query);

if (cfdb.maxcolumns != 6)
   {
   CfOut(cf_error,"","The associations database table did not promise the expected number of fields - got %d expected %d\n",cfdb.maxcolumns,6);
   CfCloseDB(&cfdb);
   return;
   }

while (CfFetchRow(&cfdb))
   {
   strncpy(from_name,CfFetchColumn(&cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_type,CfFetchColumn(&cfdb,1),CF_BUFSIZE-1);
   strncpy(from_assoc,CfFetchColumn(&cfdb,2),CF_BUFSIZE-1);
   strncpy(to_assoc,CfFetchColumn(&cfdb,3),CF_BUFSIZE-1);
   strncpy(to_type,CfFetchColumn(&cfdb,4),CF_BUFSIZE-1);
   strncpy(to_name,CfFetchColumn(&cfdb,5),CF_BUFSIZE-1);

   if (!TopicExists(tmatches,from_assoc,to_assoc))
      {
      matched++;
      AddTopic(&tmatches,from_assoc,to_assoc);
      strncpy(ctopic_type,topic_type,CF_BUFSIZE-1);
      strncpy(cfrom_assoc,from_assoc,CF_BUFSIZE-1);
      strncpy(cto_assoc,to_assoc,CF_BUFSIZE-1);
      strncpy(cto_type,to_type,CF_BUFSIZE-1);
      }

   count++;
   }

CfDeleteQuery(&cfdb);

if (matched == 0)
   {
   NothingFound(typed_topic);
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
      ShowAssociationSummary(&cfdb,tp->topic_name,tp->topic_type);
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
   ShowAssociationCosmology(&cfdb,cfrom_assoc,cto_assoc,ctopic_type,cto_type);
   }

CfCloseDB(&cfdb);
}

/*********************************************************************/

void LookupMatchingTopics(char *typed_topic)

{ char query[CF_BUFSIZE],topic_name[CF_BUFSIZE],topic_id[CF_BUFSIZE],topic_type[CF_BUFSIZE],to_type[CF_BUFSIZE];
  char from_name[CF_BUFSIZE],to_name[CF_BUFSIZE],from_assoc[CF_BUFSIZE],to_assoc[CF_BUFSIZE];
  char cfrom_assoc[CF_BUFSIZE],cto_assoc[CF_BUFSIZE],ctopic_type[CF_BUFSIZE],cto_type[CF_BUFSIZE];
  char type[CF_MAXVARSIZE],topic[CF_MAXVARSIZE];
  int  sql_database_defined = false,trymatch = false,count = 0,matched = 0;
  struct Topic *tp,*tmatches = NULL;
  CfdbConn cfdb;
  int s,e;

DeTypeTopic(typed_topic,topic,type);

CfOut(cf_verbose,"","Looking up topics matching \"%s\"\n",topic);

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
   CfOut(cf_error,"","Could not open sql_db %s\n",SQL_DATABASE);
   return;
   }

/* First assume the item is a topic */

snprintf(query,CF_BUFSIZE,"SELECT * from topics");

CfNewQueryDB(&cfdb,query);

if (cfdb.maxcolumns != 3)
   {
   CfOut(cf_error,"","The topics database table did not promise the expected number of fields - got %d expected %d\n",cfdb.maxcolumns,3);
   CfCloseDB(&cfdb);
   return;
   }

while(CfFetchRow(&cfdb))
   {
   strncpy(topic_name,CfFetchColumn(&cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_id,CfFetchColumn(&cfdb,1),CF_BUFSIZE-1);
   strncpy(topic_type,CfFetchColumn(&cfdb,2),CF_BUFSIZE-1);

   if (BlockTextCaseMatch(topic,topic_name,&s,&e))
      {
      AddTopic(&tmatches,topic_name,topic_type);
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
      ShowTopicCosmology(&cfdb,tp->topic_name,topic_id,tp->topic_type);
      }
   CfCloseDB(&cfdb);
   return;
   }

/* If no matches, try the associations */

count = 0;
matched = 0;
tmatches = NULL;

snprintf(query,CF_BUFSIZE,"SELECT * from associations");

/* Expect multiple matches always with associations */

CfNewQueryDB(&cfdb,query);

if (cfdb.maxcolumns != 6)
   {
   CfOut(cf_error,"","The associations database table did not promise the expected number of fields - got %d expected %d\n",cfdb.maxcolumns,6);
   CfCloseDB(&cfdb);
   return;
   }

while(CfFetchRow(&cfdb))
   {
   strncpy(from_name,CfFetchColumn(&cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_type,CfFetchColumn(&cfdb,1),CF_BUFSIZE-1);
   strncpy(from_assoc,CfFetchColumn(&cfdb,2),CF_BUFSIZE-1);
   strncpy(to_assoc,CfFetchColumn(&cfdb,3),CF_BUFSIZE-1);
   strncpy(to_type,CfFetchColumn(&cfdb,4),CF_BUFSIZE-1);
   strncpy(to_name,CfFetchColumn(&cfdb,5),CF_BUFSIZE-1);

   if (BlockTextCaseMatch(topic,from_assoc,&s,&e)||BlockTextCaseMatch(topic,to_assoc,&s,&e))
      {
      if (!TopicExists(tmatches,from_assoc,to_assoc))
         {
         matched++;
         AddTopic(&tmatches,from_assoc,to_assoc);
         strncpy(ctopic_type,topic_type,CF_BUFSIZE-1);
         strncpy(cfrom_assoc,from_assoc,CF_BUFSIZE-1);
         strncpy(cto_assoc,to_assoc,CF_BUFSIZE-1);
         strncpy(cto_type,to_type,CF_BUFSIZE-1);
         }
      }
   }

CfDeleteQuery(&cfdb);

if (matched == 0)
   {
   NothingFound(typed_topic);
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
      ShowAssociationSummary(&cfdb,tp->topic_name,tp->topic_type);
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
   ShowAssociationCosmology(&cfdb,cfrom_assoc,cto_assoc,ctopic_type,cto_type);
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
   CfOut(cf_verbose,"","Class promises do not have any effect here.\n");
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
/* Level                                                             */
/*********************************************************************/

void VerifyTopicPromise(struct Promise *pp)

{ char id[CF_BUFSIZE];
  char fwd[CF_BUFSIZE],bwd[CF_BUFSIZE];
  struct Attributes a;
  struct Topic *tp;

// Put all this in subfunc if LTM output specified

a = GetTopicsAttributes(pp);
 
strncpy(id,CanonifyName(pp->promiser),CF_BUFSIZE-1);

CfOut(cf_verbose,"","Attempting to install topic %s::%s\n",pp->classes,pp->promiser);

if (pp->ref != NULL)
   {
   AddCommentedTopic(&TOPIC_MAP,pp->promiser,pp->ref,pp->classes);
   }
else
   {
   AddTopic(&TOPIC_MAP,pp->promiser,pp->classes);
   }

if (tp = GetTopic(TOPIC_MAP,pp->promiser))
   {
   CfOut(cf_verbose,""," -> Topic \"%s\" installed\n",pp->promiser);
   
   if (a.fwd_name)
      {
      AddTopicAssociation(&(tp->associations),a.fwd_name,a.bwd_name,a.associates,true);
      }
   }
else
   {
   CfOut(cf_verbose,""," -> Topic \"%s\" did not install\n",pp->promiser);
   }
}

/*********************************************************************/

void VerifyOccurrencePromises(struct Promise *pp)

{ struct Attributes a;
  struct Topic *tp;
  enum representations rep_type;
  int s,e;

a = GetOccurrenceAttributes(pp);

if (a.represents == NULL)
   {
   CfOut(cf_error,"","Occurrence \"%s\" (type) promises no topics");
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

if (BlockTextMatch("[.&|]+",pp->classes,&s,&e))
   {
   CfOut(cf_error,"","Class should be a single topic for occurrences - %s does not make sense",pp->classes);
   return;
   }

if ((tp = GetCanonizedTopic(TOPIC_MAP,pp->classes)) == NULL)
   {
   CfOut(cf_error,"","Class missing - canonical identifier \"%s\" was not previously defined so we can't map it to occurrences (problem with bundlesequence?)",pp->classes);
   return;
   }
 
AddOccurrence(&(tp->occurrences),pp->classes,pp->promiser,a.represents,rep_type);
}

/*********************************************************************/

void ShowTopicLTM(FILE *fout,char *id,char *type,char *value)

{ char ntype[CF_BUFSIZE];

strncpy(ntype,Name2Id(type),CF_BUFSIZE);
 
if (strcmp(type,"any") == 0)
   {
   fprintf(fout,"[%s = \"%s\"]\n",Name2Id(id),value);
   }
else
   {
   fprintf(fout,"[%s: %s = \"%s\"]\n",Name2Id(id),ntype,value);
   }
}

/*********************************************************************/

void ShowAssociationsLTM(FILE *fout,char *from_id,char *from_type,struct TopicAssociation *ta)

{ char assoc_id[CF_BUFSIZE];

strncpy(assoc_id,CanonifyName(ta->fwd_name),CF_BUFSIZE);

if (from_id == NULL)
   {
   fprintf(fout,"\n[%s = \"%s\" \n",assoc_id,ta->fwd_name);
   
   if (ta->associate_topic_type)
      {
      fprintf(fout,"          = \"%s\" / %s]\n\n",ta->bwd_name,ta->associate_topic_type);
      }
   else
      {
      fprintf(fout," = \"%s\" / unknown_association_counterpart]\n",ta->bwd_name);
      }
   }
else
   {
   struct Rlist *rp;
   char to_id[CF_BUFSIZE],*to_type;
   
   to_type = ta->associate_topic_type;

   if (!to_type)
      {
      to_type = "any";
      }

   for (rp = ta->associates; rp != NULL; rp=rp->next)
      {
      strncpy(to_id,CanonifyName(rp->item),CF_BUFSIZE);

      fprintf(fout,"%s( %s : %s, %s : %s)\n",assoc_id,CanonifyName(from_id),from_type,to_id,to_type);
      }
   }
}

/*********************************************************************/

void ShowOccurrencesLTM(FILE *fout,char *topic_id,struct Occurrence *op)

{ struct Rlist *rp;
  char subtype[CF_BUFSIZE];

fprintf(fout,"\n /* occurrences of %s */\n\n",topic_id);
 
for (rp = op->represents; rp != NULL; rp=rp->next)
   {
   strncpy(subtype,CanonifyName(rp->item),CF_BUFSIZE-1);

   switch (op->rep_type)
      {
      case cfk_url:   
          fprintf(fout,"{%s,%s, \"%s\"}\n",topic_id,subtype,op->locator);
          break;

      case cfk_web:   
          fprintf(fout,"{%s,%s, \"%s\"}\n",topic_id,subtype,op->locator);
          break;
          
      case cfk_file:
      case cfk_db:
      case cfk_literal:
          fprintf(fout,"{%s,%s, [[%s]]}\n",topic_id,subtype,op->locator);
          break;

      case cfk_image:
          fprintf(fout,"{%s,%s,\"Image at %s\"}\n",topic_id,subtype,op->locator);
          break;
      }

   if (!GetCanonizedTopic(TOPIC_MAP,subtype))
      {
      CfOut(cf_inform,"","Occurrence of %s makes reference to a sub-topic %s but that has not (et) been defined",topic_id,subtype);
      }
   }
}

/*********************************************************************/

void GenerateSQL()

{ struct Topic *tp;
  struct TopicAssociation *ta;
  struct Occurrence *op;
  FILE *fout = stdout;
  char filename[CF_BUFSIZE],longname[CF_BUFSIZE],query[CF_BUFSIZE],safe[CF_BUFSIZE];
  struct Rlist *rp;
  int i,sql_database_defined = false;
  CfdbConn cfdb;

AddSlash(BUILD_DIR);
snprintf(filename,CF_BUFSIZE-1,"%stopic_map.sql",BUILD_DIR);

if (WRITE_SQL && strlen(SQL_OWNER) > 0)
   {
   sql_database_defined = true;
   }

CfOut(cf_verbose,"","Writing %s\n",filename);

/* Open channels */

if ((fout = fopen(filename,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","Cannot write to %s\n",filename);
   return;
   }

if (sql_database_defined)
   {
   CfConnectDB(&cfdb,SQL_TYPE,SQL_SERVER,SQL_OWNER,SQL_PASSWD,SQL_DATABASE);

   if (!cfdb.connected)
      {
      CfOut(cf_error,"","Could not open sql_db %s\n",SQL_DATABASE);
      return;
      }
   }
else
   {
   cfdb.connected = false;
   }

/* Schema - very simple */

fprintf(fout,"# CREATE DATABASE IF NOT EXISTS cf_topic_map\n");
fprintf(fout,"# USE %s_topic_map\n",TM_PREFIX);
 
snprintf(query,CF_BUFSIZE-1,
        "CREATE TABLE topics"
        "("
        "topic_name varchar(256),"
        "topic_id varchar(256),"
        "topic_type varchar(256)"
        ");\n"
        );

fprintf(fout,"%s",query);

snprintf(query,CF_BUFSIZE-1,
        "CREATE TABLE associations"
        "("
        "from_name varchar(256),"
        "from_type varchar(256),"
        "from_assoc varchar(256),"
        "to_assoc varchar(256),"
        "to_type varchar(256),"
        "to_name varchar(256)"
        ");\n"
        );

fprintf(fout,"%s",query);

snprintf(query,CF_BUFSIZE-1,
        "CREATE TABLE occurrences"
        "("
        "topic_name varchar(256),"
        "locator varchar(256),"
        "locator_type varchar(256),"
        "subtype varchar(256)"
        ");\n"
        );

fprintf(fout,"%s",query);

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

for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {
   snprintf(longname,CF_BUFSIZE,"%s",GetLongTopicName(&cfdb,TOPIC_MAP,tp->topic_name));
   strncpy(safe,EscapeSQL(&cfdb,longname),CF_BUFSIZE);

   snprintf(query,CF_BUFSIZE-1,"INSERT INTO topics (topic_name,topic_id,topic_type) values ('%s','%s','%s');\n",safe,Name2Id(tp->topic_name),tp->topic_type);
   fprintf(fout,"%s",query);
   CfVoidQueryDB(&cfdb,query);
   }

/* Associations */

for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {
   snprintf(longname,CF_BUFSIZE,"%s",GetLongTopicName(&cfdb,TOPIC_MAP,tp->topic_name));
   strncpy(safe,EscapeSQL(&cfdb,longname),CF_BUFSIZE);

   for (ta = tp->associations; ta != NULL; ta=ta->next)
      {
      for (rp = ta->associates; rp != NULL; rp=rp->next)
         {
         snprintf(query,CF_BUFSIZE-1,"INSERT INTO associations (from_name,to_name,from_assoc,to_assoc,from_type,to_type) values ('%s','%s','%s','%s','%s','%s');\n",safe,GetLongTopicName(&cfdb,TOPIC_MAP,rp->item),ta->fwd_name,ta->bwd_name,tp->topic_type,ta->associate_topic_type);
         fprintf(fout,"%s",query);
         CfVoidQueryDB(&cfdb,query);
         }
      }
   }

/* Occurrences */

for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {
   snprintf(longname,CF_BUFSIZE,"%s",GetLongTopicName(&cfdb,TOPIC_MAP,tp->topic_name));
   strncpy(safe,EscapeSQL(&cfdb,longname),CF_BUFSIZE);

   for (op = tp->occurrences; op != NULL; op=op->next)
      {
      for (rp = op->represents; rp != NULL; rp=rp->next)
         {
         char safeexpr[CF_BUFSIZE];
         strcpy(safeexpr,EscapeSQL(&cfdb,op->locator));

         snprintf(query,CF_BUFSIZE-1,"INSERT INTO occurrences (topic_name,locator,locator_type,subtype) values ('%s','%s','%d','%s')\n",CanonifyName(tp->topic_name),safeexpr,op->rep_type,GetLongTopicName(&cfdb,TOPIC_MAP,rp->item));
         fprintf(fout,"%s",query);
         CfVoidQueryDB(&cfdb,query);
         }
      }
   }


/* Close channels */

fclose(fout);

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
#ifdef HAVE_LIBGVC
struct Rlist *semantics = NULL;

if (GRAPH)
   {
   VerifyGraph(TOPIC_MAP,NULL,NULL);

#ifdef HAVE_LIBCFNOVA   
   PrependRScalar(&semantics,NOVA_GIVES,CF_SCALAR);
   PrependRScalar(&semantics,NOVA_USES,CF_SCALAR);
   VerifyGraph(TOPIC_MAP,semantics,"flow");
#endif
   }

DeleteRlist(semantics);
#endif
}

/*********************************************************************/

void ShowTopicDisambiguation(CfdbConn *cfdb,struct Topic *tmatches,char *name)

{ char banner[CF_BUFSIZE];
  struct Topic *tp;
 
if (HTML)
   {
   snprintf(banner,CF_BUFSIZE,"Disambiguation %s",name);
   CfHtmlHeader(stdout,banner,STYLESHEET,WEBDRIVER,BANNER);
   printf("<div id=\"intro\">");

   printf("<p>More than one topic matched your search parameters.</p>");

   printf("<ul>");
   
   for (tp = tmatches; tp != NULL; tp=tp->next)
      {
      printf("<li>Topic \"%s\" found ",NextTopic(tp->topic_name,tp->topic_type));
      printf("in the context of \"%s\"\n",NextTopic(tp->topic_type,""));
      }

   printf("</ul>");
   printf("</div>\n");
   CfHtmlFooter(stdout,FOOTER);
   }
else
   {
   for (tp = tmatches; tp != NULL; tp=tp->next)
      {
      printf("Topic \"%s\" found in the context of \"%s\"\n",tp->topic_name,tp->topic_type);
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
  char topic_name[CF_BUFSIZE],topic_id[CF_BUFSIZE],topic_type[CF_BUFSIZE],associate[CF_BUFSIZE];
  char query[CF_BUFSIZE],fassociation[CF_BUFSIZE],bassociation[CF_BUFSIZE],safe[CF_BUFSIZE];
  char locator[CF_BUFSIZE],subtype[CF_BUFSIZE],to_type[CF_BUFSIZE];
  enum representations locator_type;
  struct Rlist *rp;

/* Collect data - first other topics of same type */

snprintf(query,CF_BUFSIZE,"SELECT * from topics where topic_type='%s' order by topic_name desc",this_type);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 3)
   {
   CfOut(cf_error,"","The topics database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,3);
   return;
   }

while(CfFetchRow(cfdb))
   {
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_id,CfFetchColumn(cfdb,1),CF_BUFSIZE-1);
   strncpy(topic_type,CfFetchColumn(cfdb,2),CF_BUFSIZE-1);

   if (strcmp(topic_name,this_name) == 0 || strcmp(topic_id,this_name) == 0)
      {
      continue;
      }

   AddTopic(&other_topics,topic_name,topic_type);
   }

CfDeleteQuery(cfdb);

/* Collect data - then topics of this topic-type */

snprintf(query,CF_BUFSIZE,"SELECT * from topics where topic_type='%s' order by topic_name desc",CanonifyName(this_name));

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 3)
   {
   CfOut(cf_error,"","The topics database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,3);
   return;
   }

while(CfFetchRow(cfdb))
   {
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_id,CfFetchColumn(cfdb,1),CF_BUFSIZE-1);
   strncpy(topic_type,CfFetchColumn(cfdb,2),CF_BUFSIZE-1);

   if (strcmp(topic_name,this_name) == 0 || strcmp(topic_id,this_name) == 0)
      {
      continue;
      }

   AddTopic(&topics_this_type,topic_name,topic_type);
   }

CfDeleteQuery(cfdb);

/* Then associated topics */

snprintf(query,CF_BUFSIZE,"SELECT * from associations where to_name='%s'",this_name);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 6)
   {
   CfOut(cf_error,"","The associations database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,6);
   return;
   }

while(CfFetchRow(cfdb))
   {
   struct Rlist *this = NULL;
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_type,CfFetchColumn(cfdb,1),CF_BUFSIZE-1);
   strncpy(fassociation,CfFetchColumn(cfdb,2),CF_BUFSIZE-1);
   strncpy(bassociation,CfFetchColumn(cfdb,3),CF_BUFSIZE-1);
   strncpy(to_type,CfFetchColumn(cfdb,4),CF_BUFSIZE-1);
   strncpy(associate,CfFetchColumn(cfdb,5),CF_BUFSIZE-1);
   AppendRlist(&this,TypedTopic(topic_name,topic_type),CF_SCALAR);
   AddTopicAssociation(&associations,bassociation,NULL,this,false);
   DeleteRlist(this);
   }

CfDeleteQuery(cfdb);

snprintf(query,CF_BUFSIZE,"SELECT * from associations where from_name='%s'",this_name);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 6)
   {
   CfOut(cf_error,"","The associations database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,6);
   return;
   }

while(CfFetchRow(cfdb))
   {
   struct Rlist *this = NULL;
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_type,CfFetchColumn(cfdb,1),CF_BUFSIZE-1);
   strncpy(fassociation,CfFetchColumn(cfdb,2),CF_BUFSIZE-1);
   strncpy(bassociation,CfFetchColumn(cfdb,3),CF_BUFSIZE-1);
   strncpy(to_type,CfFetchColumn(cfdb,4),CF_BUFSIZE-1);
   strncpy(associate,CfFetchColumn(cfdb,5),CF_BUFSIZE-1);

   AppendRlist(&this,associate,CF_SCALAR);
   AddTopicAssociation(&associations,fassociation,NULL,this,false);
   DeleteRlist(this);
   }

CfDeleteQuery(cfdb);

/* Finally occurrences of the mentioned topic */

strncpy(safe,EscapeSQL(cfdb,this_name),CF_BUFSIZE);
snprintf(query,CF_BUFSIZE,"SELECT * from occurrences where topic_name='%s' or subtype='%s' order by locator_type",this_id,safe);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 4)
   {
   CfOut(cf_error,"","The occurrences database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,4);
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
   
   AddOccurrence(&occurrences,topic_name,locator,this,locator_type);
   DeleteRlist(this);
   }

CfDeleteQuery(cfdb);

/* Now print the results */

if (HTML)
   {
   ShowHtmlResults(this_name,this_type,other_topics,associations,occurrences,topics_this_type);
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

{ char topic_name[CF_BUFSIZE],topic_id[CF_BUFSIZE],topic_type[CF_BUFSIZE],query[CF_BUFSIZE];
  char banner[CF_BUFSIZE],output[CF_BUFSIZE];
  char from_name[CF_BUFSIZE],to_name[CF_BUFSIZE],from_assoc[CF_BUFSIZE],to_assoc[CF_BUFSIZE];
  struct Rlist *flist = NULL,*tlist = NULL,*ftlist = NULL,*ttlist = NULL,*rp;
  int count = 0;
 

/* Role players - fwd search */

snprintf(query,CF_BUFSIZE,"SELECT * from associations where from_assoc='%s'",EscapeSQL(cfdb,this_fassoc));

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 6)
   {
   CfOut(cf_error,"","The associations database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,6);
   CfCloseDB(cfdb);
   return;
   }

while(CfFetchRow(cfdb))
   {
   strncpy(from_name,CfFetchColumn(cfdb,0),CF_BUFSIZE-1);
   strncpy(topic_type,CfFetchColumn(cfdb,1),CF_BUFSIZE-1);
   strncpy(from_assoc,CfFetchColumn(cfdb,2),CF_BUFSIZE-1);
   strncpy(to_assoc,CfFetchColumn(cfdb,3),CF_BUFSIZE-1);
   strncpy(to_type,CfFetchColumn(cfdb,4),CF_BUFSIZE-1);
   strncpy(to_name,CfFetchColumn(cfdb,5),CF_BUFSIZE-1);

   if (HTML)
      {
      snprintf(output,CF_BUFSIZE,"<li>%s (%s)</li>\n",NextTopic(from_name,topic_type),topic_type);
      }
   else
      {
      snprintf(output,CF_BUFSIZE,"    %s (%s)\n",from_name,topic_type);
      }

   IdempPrependRScalar(&flist,output,CF_SCALAR);
   IdempPrependRScalar(&ftlist,topic_type,CF_SCALAR);

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
   snprintf(query,CF_BUFSIZE,"SELECT * from associations where to_assoc='%s'",EscapeSQL(cfdb,this_tassoc));

   CfNewQueryDB(cfdb,query);
   
   if (cfdb->maxcolumns != 6)
      {
      CfOut(cf_error,"","The associations database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,6);
      CfCloseDB(cfdb);
      return;
      }
   
   while(CfFetchRow(cfdb))
      {
      strncpy(from_name,CfFetchColumn(cfdb,0),CF_BUFSIZE-1);
      strncpy(topic_type,CfFetchColumn(cfdb,1),CF_BUFSIZE-1);
      strncpy(from_assoc,CfFetchColumn(cfdb,2),CF_BUFSIZE-1);
      strncpy(to_assoc,CfFetchColumn(cfdb,3),CF_BUFSIZE-1);
      strncpy(to_type,CfFetchColumn(cfdb,4),CF_BUFSIZE-1);
      strncpy(to_name,CfFetchColumn(cfdb,5),CF_BUFSIZE-1);

      if (HTML)
         {
         snprintf(output,CF_BUFSIZE,"<li>%s (%s)</li>\n",NextTopic(from_name,topic_type),topic_type);
         }
      else
         {
         snprintf(output,CF_BUFSIZE,"    %s (%s)\n",from_name,from_type);
         }
      
      IdempPrependRScalar(&flist,output,CF_SCALAR);
      IdempPrependRScalar(&ftlist,topic_type,CF_SCALAR);
      
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


printf("\nTopics of the type %s:\n\n",this_name);

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

printf("\nOccurrences of this topic:\n\n");

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
          printf(" %s (URL)",oc->locator);
          break;
      case cfk_file:
          printf("%s (file)",oc->locator);
          break;
      case cfk_db:
          printf(" %s (DB)",oc->locator);
          break;          
      case cfk_literal:
          printf(" \"%s\" (Text)",oc->locator);
          break;
      case cfk_image:
          printf("(Image)",oc->locator);
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
  int count = 0;
  FILE *fout = stdout;
  char banner[CF_BUFSIZE],filename[CF_BUFSIZE];
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

snprintf(banner,CF_BUFSIZE,"Topic: %s",this_name);  
CfHtmlHeader(stdout,banner,STYLESHEET,WEBDRIVER,BANNER);

snprintf(filename,CF_BUFSIZE,"graphs/%s.png",CanonifyName(TypedTopic(this_name,this_type)));

if (stat(filename,&sb) != -1)
   {
   fprintf(fout,"<div id=\"image\"><a href=\"%s\" target=\"_blank\"><img src=\"%s\"></a></div>",filename,filename);
   }

snprintf(filename,CF_BUFSIZE,"graphs/flow_%s.png",CanonifyName(TypedTopic(this_name,this_type)));

if (stat(filename,&sb) != -1)
   {
   fprintf(fout,"<div id=\"image\"><a href=\"%s\" target=\"_blank\"><img src=\"%s\"></a></div>",filename,filename);
   }

// div id=content_container

fprintf(fout,"<div id=\"intro\">");
fprintf(fout,"This topic \"%s\" has type ",NextTopic(this_name,this_type));
fprintf(fout,"\"%s\" in map version %s</div>\n",NextTopic(this_type,""),v);

if (occurrences != NULL)
   {
   count = 0;
   
   fprintf(fout,"<p><div id=\"occurrences\">");
   
   CfOut(cf_error,"","\n<h2>Occurrences of this topic:</h2>\n\n");
   
   fprintf(fout,"<ul>\n");
   
   for (oc = occurrences; oc != NULL; oc=oc->next)
      {
      if (oc->represents == NULL)
         {
         fprintf(fout,"<li>(directly)");
         }
      else
         {
         fprintf(fout,"<li>");
	 
         for (rp = oc->represents; rp != NULL; rp=rp->next)
            {
            fprintf(fout,"%s, ",NextTopic((char *)rp->item,""));
            }
         
         fprintf(fout,":");
         }
      
      switch (oc->rep_type)
         {
         case cfk_url:
             fprintf(fout,"<span id=\"url\"><a href=\"%s\">%s</a> </span>(URL)",oc->locator,oc->locator);
             break;
         case cfk_web:
             fprintf(fout,"<span id=\"url\"><a href=\"%s\"> ...%s</a> </span>(URL)",oc->locator,URLHint(oc->locator));
             break;
         case cfk_file:
             fprintf(fout," <a href=\"file://%s\">%s</a> (file)",oc->locator,oc->locator);
             break;
         case cfk_db:
             fprintf(fout," %s (DB)",oc->locator);
             break;          
         case cfk_literal:
             fprintf(fout,"<p> \"%s\" (Text)</p>",oc->locator);
             break;
         case cfk_image:
             fprintf(fout,"<p><div id=\"embedded_image\"><a href=\"%s\"><img src=\"%s\"></a></div></p>",oc->locator,oc->locator);
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
   fprintf(fout,"<i>This topic has no direct documentation</i>");
   fprintf(fout,"</div>");
   }

if (associations)
   {
   count = 0;
   
   fprintf(fout,"<p><div id=\"associations\">");
   fprintf(fout,"\n<h2>Associated with this:</h2>\n\n");
   
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

if (topics_this_type)
   {
   count = 0;
   
   fprintf(fout,"<p><div id=\"thistype\">");
   fprintf(fout,"\n<h2>Topics of type %s:</h2>\n\n",NextTopic(this_name,this_type));
   
   fprintf(fout,"<ul>\n");
   
   for (tp = topics_this_type; tp != NULL; tp=tp->next)
      {
      fprintf(fout,"<li>  %s \n",NextTopic(tp->topic_name,tp->topic_type));
      count++;
      }
   
   if (count == 0)
      {
      fprintf(fout,"<li>    (none)\n");
      }
   
   fprintf(fout,"</ul>\n");
   fprintf(fout,"</div>");
   }

if (other_topics)
   {
   count = 0;
   
   fprintf(fout,"<p><div id=\"others\">\n");
   
   fprintf(fout,"\n<h2>Other topics of type %s:</h2>\n\n",this_type);
   
   fprintf(fout,"<ul>\n");
   
   for (tp = other_topics; tp != NULL; tp=tp->next)
      {
      fprintf(fout,"<li>  %s \n",NextTopic(tp->topic_name,tp->topic_type));
      count++;
      }
   
   if (count == 0)
      {
      fprintf(fout,"<li>    (none)\n");
      }
   
   fprintf(fout,"</div>");
   
   fprintf(fout,"</ul>\n");
   }

// end container

CfHtmlFooter(stdout,FOOTER);
}

/*********************************************************************/

char *NextTopic(char *topic,char *type)

{ static char url[CF_BUFSIZE];
  char ctopic[CF_MAXVARSIZE],ctype[CF_MAXVARSIZE];

if (strlen(WEBDRIVER) == 0)
   {
   CfOut(cf_error,"","No query_engine is defined\n");
   exit(1);
   }

if (strchr(topic,':'))
   {
   DeTypeTopic(topic,ctopic,ctype);    
   snprintf(url,CF_BUFSIZE,"<a href=\"%s?next=%s\">%s</a>",WEBDRIVER,topic,ctopic);
   }
else
   {
   snprintf(url,CF_BUFSIZE,"<a href=\"%s?next=%s::%s\">%s</a>",WEBDRIVER,type,topic,topic);
   }

return url;
}


/* EOF */




