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
void LookupTopic(char *topic);
void ShowTopicSummary(CfdbConn *cfdb,char *topic_name,char *topic_type);
void ShowTopicCosmology(CfdbConn *cfdb,char *topic_name,char *topic_type);
void ShowAssociationSummary(CfdbConn *cfdb,char *this_fassoc,char *this_tassoc);
void ShowAssociationCosmology(CfdbConn *cfdb,char *this_fassoc,char *this_tassoc,char *from_type,char *to_type);
char *Name2Id(char *s);
void ShowTextResults(char *name,char *type,struct Topic *other_topics,struct TopicAssociation *associations,struct Occurrence *occurrences);
void ShowHtmlResults(char *name,char *type,struct Topic *other_topics,struct TopicAssociation *associations,struct Occurrence *occurrences);
char *NextTopic(char *link);

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

struct Topic *TOPIC_MAP = NULL;

char TM_PREFIX[CF_MAXVARSIZE];
char BUILD_DIR[CF_BUFSIZE];
char SQL_DATABASE[CF_MAXVARSIZE];
char SQL_OWNER[CF_MAXVARSIZE];
char SQL_PASSWD[CF_MAXVARSIZE];
char SQL_SERVER[CF_MAXVARSIZE];
char TOPIC_CMD[CF_MAXVARSIZE];
char WEBDRIVER[CF_MAXVARSIZE];
char STYLESHEET[CF_MAXVARSIZE];
enum cfdbtype SQL_TYPE = cfd_notype;
int HTML = false;
int WRITE_SQL = false;

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

  /* GNU STUFF FOR LATER #include "getopt.h" */
 
 struct option OPTIONS[12] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "version",no_argument,0,'V' },
      { "define",required_argument,0,'D' },
      { "negate",required_argument,0,'N' },
      { "file",required_argument,0,'f' },
      { "syntax",no_argument,0,'S'},
      { "parse-only",no_argument,0,'p'},
      { "topic",required_argument,0,'t'},
      { "sql",no_argument,0,'s'},
      { NULL,0,0,'\0' }
      };

/*****************************************************************************/

int main(int argc,char *argv[])

{
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
   }
else
   {
   LookupTopic(TOPIC_CMD);
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
 
while ((c=getopt_long(argc,argv,"hHd:vVf:pD:N:Sst:w:",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

          strncpy(VINPUTFILE,optarg,CF_BUFSIZE-1);
          VINPUTFILE[CF_BUFSIZE-1] = '\0';
          MINUSF = true;
          break;

      case 'd': 
          AddClassToHeap("opt_debug");
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
             case '3':
                 D3 = true;
                 DEBUG = true;
                 VERBOSE = true;
                 break;
             case '4':
                 D4 = true;
                 DEBUG = true;
                 break;
             default:
                 DEBUG = true;
                 break;
             }
          break;
          
      case 'D':
          AddMultipleClasses(optarg);
          break;
          
      case 'N':
          NegateCompoundClass(optarg,&VNEGHEAP);
          break;

      case 't':
          strcpy(TOPIC_CMD,optarg);
          break;

      case 's':
          WRITE_SQL = true;
          break;
          
      case 'v': VERBOSE = true;
          break;
          
      case 'p': PARSEONLY = true;
          IGNORELOCK = true;
          break;          

      case 'V': Version("Knowledge agent");
          exit(0);
          
      case 'h': Syntax("Knowledge agent");
          exit(0);

      default: Syntax("Knowledge agent");
          exit(1);
          
      }
  }
}

/*****************************************************************************/

void ThisAgentInit()

{ char vbuff[CF_BUFSIZE];

strcpy(TM_PREFIX,"");
strcpy(WEBDRIVER,"");
strcpy(BUILD_DIR,".");
strcpy(SQL_DATABASE,"cf_topic_map");
strcpy(SQL_OWNER,"");
strcpy(SQL_PASSWD,"");
strcpy(SQL_SERVER,"localhost");
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

      BannerSubType(bp->name,sp->name);

      for (pp = sp->promiselist; pp != NULL; pp=pp->next)
         {
         ExpandPromise(cf_agent,bp->name,pp,KeepKnowledgePromise);
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
         
         /* First populator sets the reverse type others must conform to */
         
         if (ta->associate_topic_type == NULL)
            {
            if (reverse_type == NULL)
               {
               CfOut(cf_error,"","Topic \"%s\" is not declared with a type",rp->item);
               continue;
               }

            if ((ta->associate_topic_type = strdup(CanonifyName(reverse_type))) == NULL)
               {
               CfOut(cferror,"malloc","Memory failure in AddTopicAssociation");
               FatalError("");
               }
            }
         
         if (ta->associate_topic_type && strcmp(ta->associate_topic_type,reverse_type) != 0)
            {
            CfOut(cf_error,"","Associates in the relationship \"%s\" do not all have the same topic type",ta->fwd_name);
            CfOut(cf_error,"","Found \"%s\" but expected \"%s\"",ta->associate_topic_type,reverse_type);
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

Verbose("Writing %s\n",filename);

if ((fout = fopen(filename,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","Cannot write to %s\n",filename);
   return;
   }

for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {
   for (ta = tp->associations; ta != NULL; ta=ta->next)
      {
      snprintf(longname,CF_BUFSIZE,"%s/%s",ta->fwd_name,ta->bwd_name);
      
      if (!IsItemIn(generic_associations,longname))
         {
         PrependItemList(&generic_associations,longname);            
         }
      }

   if (tp->comment)
      {
      snprintf(longname,CF_BUFSIZE,"%s (%s)",tp->comment,tp->topic_name);
      }
   else
      {
      snprintf(longname,CF_BUFSIZE,"%s",tp->topic_name);
      }

   if (!IsItemIn(generic_topics,longname))
      {
      PrependItemList(&generic_topics,longname);            
      }

   if (!IsItemIn(generic_types,tp->topic_type))
      {
      PrependItemList(&generic_types,tp->topic_type);            
      }
   }

fprintf(fout,"<html>\n");
fprintf(fout,"<head>\n");
fprintf(fout,"<link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/syntax.css\" />\n");
fprintf(fout,"<link rel=\"stylesheet\" type=\"text/css\" href=\"http://www.cfengine.org/cf_blue.css\"/>\n");
fprintf(fout,"</head>\n");
fprintf(fout,"<body>\n");

fprintf(fout,"<h1>Cfengine Operational Ontology</h1><p>");

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

fprintf(fout,"</body>\n");
fprintf(fout,"</html>");
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

Verbose("Writing %s\n",filename);

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

void LookupTopic(char *topic)

{ char query[CF_BUFSIZE],topic_name[CF_MAXVARSIZE],topic_id[CF_MAXVARSIZE],topic_type[CF_MAXVARSIZE],to_type[CF_MAXVARSIZE];
  char from_name[CF_BUFSIZE],to_name[CF_MAXVARSIZE],from_assoc[CF_MAXVARSIZE],to_assoc[CF_MAXVARSIZE];
  int sql_database_defined = false,trymatch = false,count = 0,matched = 0;
  struct Topic *tp,*tmatches = NULL;
  CfdbConn cfdb;
    
Verbose("Looking up topics matching \"%s\"\n",topic);

/* We need to set a scope for the regex stuff */

NewScope("this");

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

if (IsRegex(topic))
   {
   snprintf(query,CF_BUFSIZE,"SELECT * from topics");
   trymatch = true;
   }
else
   {
   snprintf(query,CF_BUFSIZE,"SELECT * from topics where topic_name='%s' or topic_id='%s'",topic,topic);
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
   strncpy(topic_name,CfFetchColumn(&cfdb,0),CF_MAXVARSIZE-1);
   strncpy(topic_id,CfFetchColumn(&cfdb,1),CF_MAXVARSIZE-1);
   strncpy(topic_type,CfFetchColumn(&cfdb,2),CF_MAXVARSIZE-1);
   
   if (trymatch && FullTextMatch(topic,topic_name))
      {
      AddTopic(&tmatches,topic_name,topic_type);
      matched++;
      }

   count++;
   }

CfDeleteQuery(&cfdb);

if (matched > 1)
   {
   /* End summary */
   for (tp = tmatches; tp != NULL; tp=tp->next)
      {
      ShowTopicSummary(&cfdb,tp->topic_name,tp->topic_type);
      }
   CfCloseDB(&cfdb);
   return;
   }

if (count == 1)
   {
   /* End single match */
   ShowTopicCosmology(&cfdb,topic_name,topic_type);
   CfCloseDB(&cfdb);
   return;
   }

/* If no matches, try the associations */

count = 0;
matched = 0;

if (IsRegex(topic))
   {
   snprintf(query,CF_BUFSIZE,"SELECT * from associations");
   trymatch = true;
   }
else
   {
   snprintf(query,CF_BUFSIZE,"SELECT * from associations where from_assoc='%s' or to_assoc='%s'",topic,topic);
   trymatch = false;
   }

CfNewQueryDB(&cfdb,query);

if (cfdb.maxcolumns != 6)
   {
   CfOut(cf_error,"","The associations database table did not promise the expected number of fields - got %d expected %d\n",cfdb.maxcolumns,6);
   CfCloseDB(&cfdb);
   return;
   }

while(CfFetchRow(&cfdb))
   {
   strncpy(from_name,CfFetchColumn(&cfdb,0),CF_MAXVARSIZE-1);
   strncpy(topic_type,CfFetchColumn(&cfdb,1),CF_MAXVARSIZE-1);
   strncpy(from_assoc,CfFetchColumn(&cfdb,2),CF_MAXVARSIZE-1);
   strncpy(to_assoc,CfFetchColumn(&cfdb,3),CF_MAXVARSIZE-1);
   strncpy(to_type,CfFetchColumn(&cfdb,4),CF_MAXVARSIZE-1);
   strncpy(to_name,CfFetchColumn(&cfdb,5),CF_MAXVARSIZE-1);

   if (trymatch && (FullTextMatch(topic,from_assoc)||FullTextMatch(topic,to_assoc)))
      {
      if (!TopicExists(tmatches,from_assoc,to_assoc))
         {
         matched++;
         AddTopic(&tmatches,from_assoc,to_assoc);
         }
      }

   count++;
   }

CfDeleteQuery(&cfdb);

if (count == 0)
   {
   printf("Nothing found for \"%s\"\n",topic);
   return;
   }

if (matched > 1)
   {
   /* End assoc summary */
   
   for (tp = tmatches; tp != NULL; tp=tp->next)
      {
      ShowAssociationSummary(&cfdb,tp->topic_name,tp->topic_type);
      }
   
   CfCloseDB(&cfdb);
   return;
   }

if (count == 1 || matched == 1)
   {
   ShowAssociationCosmology(&cfdb,from_assoc,to_assoc,topic_type,to_type);
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
   if (!IsDefinedClass(pp->classes))
      {
      Verbose("\n");
      Verbose(". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
      Verbose("Skipping whole next promise, as context %s is not valid\n",pp->classes);
      Verbose(". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
      return;
      }

   KeepClassContextPromise(pp);
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

{ char id[CF_MAXVARSIZE];
  char fwd[CF_MAXVARSIZE],bwd[CF_MAXVARSIZE];
  struct Attributes a;
  struct Topic *tp;

// Put all this in subfunc if LTM output specified

a = GetTopicsAttributes(pp);
 
strncpy(id,CanonifyName(pp->promiser),CF_MAXVARSIZE-1);

if (pp->ref != NULL)
   {
   AddCommentedTopic(&TOPIC_MAP,pp->promiser,pp->ref,pp->classes);
   }
else
   {
   AddTopic(&TOPIC_MAP,pp->promiser,pp->classes);
   }

tp = GetTopic(TOPIC_MAP,pp->promiser);

if (a.fwd_name)
   {
   AddTopicAssociation(&(tp->associations),a.fwd_name,a.bwd_name,tp->topic_type,a.associates,true);
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

{ char assoc_id[CF_MAXVARSIZE];

strncpy(assoc_id,CanonifyName(ta->fwd_name),CF_MAXVARSIZE);

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
   char to_id[CF_MAXVARSIZE],*to_type;
   
   to_type = ta->associate_topic_type;

   if (!to_type)
      {
      to_type = "any";
      }

   for (rp = ta->associates; rp != NULL; rp=rp->next)
      {
      strncpy(to_id,CanonifyName(rp->item),CF_MAXVARSIZE);

      fprintf(fout,"%s( %s : %s, %s : %s)\n",assoc_id,CanonifyName(from_id),from_type,to_id,to_type);
      }
   }
}

/*********************************************************************/

void ShowOccurrencesLTM(FILE *fout,char *topic_id,struct Occurrence *op)

{ struct Rlist *rp;
  char subtype[CF_MAXVARSIZE];

fprintf(fout,"\n /* occurrences of %s */\n\n",topic_id);
 
for (rp = op->represents; rp != NULL; rp=rp->next)
   {
   strncpy(subtype,CanonifyName(rp->item),CF_MAXVARSIZE-1);

   switch (op->rep_type)
      {
      case cfk_url:   
          fprintf(fout,"{%s,%s, \"%s\"}\n",topic_id,subtype,op->locator);
          break;

      case cfk_file:
      case cfk_db:
      case cfk_literal:
          fprintf(fout,"{%s,%s, [[%s]]}\n",topic_id,subtype,op->locator);
          break;
      }

   if (!GetCanonizedTopic(TOPIC_MAP,subtype))
      {
      CfOut(cf_error,"","Occurrence of %s makes reference to a sub-topic %s but that has not been defined",topic_id,subtype);
      }
   }
}

/*********************************************************************/

void GenerateSQL()

{ struct Topic *tp;
  struct TopicAssociation *ta;
  struct Occurrence *op;
  FILE *fout = stdout;
  char filename[CF_BUFSIZE],longname[CF_BUFSIZE],query[CF_BUFSIZE];
  struct Rlist *rp;
  int i,sql_database_defined = false;
  CfdbConn cfdb;

AddSlash(BUILD_DIR);
snprintf(filename,CF_BUFSIZE-1,"%stopic_map.sql",BUILD_DIR);

if (WRITE_SQL && strlen(SQL_OWNER) > 0)
   {
   sql_database_defined = true;
   }

Verbose("Writing %s\n",filename);

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
        "topic_name varchar(128),"
        "topic_id varchar(128),"
        "topic_type varchar(128)"
        ");\n"
        );

fprintf(fout,query);
        
snprintf(query,CF_BUFSIZE-1,
        "CREATE TABLE associations"
        "("
        "from_name varchar(128),"
        "from_type varchar(128),"
        "from_assoc varchar(128),"
        "to_assoc varchar(128),"
        "to_type varchar(128),"
        "to_name varchar(128)"
        ");\n"
        );

fprintf(fout,query);

snprintf(query,CF_BUFSIZE-1,
        "CREATE TABLE occurrences"
        "("
        "topic_name varchar(128),"
        "locator varchar(128),"
        "locator_type varchar(128),"
        "subtype varchar(128)"
        ");\n"
        );

fprintf(fout,query);

/* Delete existing data and recreate */

snprintf(query,CF_BUFSIZE-1,"delete from topics\n",TM_PREFIX);
fprintf(fout,query);
CfVoidQueryDB(&cfdb,query);
snprintf(query,CF_BUFSIZE-1,"delete from associations\n",TM_PREFIX);
fprintf(fout,query);
CfVoidQueryDB(&cfdb,query);
snprintf(query,CF_BUFSIZE-1,"delete from occurrences\n",TM_PREFIX);
fprintf(fout,query);
CfVoidQueryDB(&cfdb,query);

/* Class types */

for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {
   if (tp->comment)
      {
      snprintf(longname,CF_BUFSIZE,"%s (%s)",tp->comment,tp->topic_name);
      }
   else
      {
      snprintf(longname,CF_BUFSIZE,"%s",tp->topic_name);
      }

   snprintf(query,CF_BUFSIZE-1,"INSERT INTO topics (topic_name,topic_id,topic_type) values ('%s','%s','%s');\n",longname,Name2Id(tp->topic_name),tp->topic_type);
   fprintf(fout,query);
   CfVoidQueryDB(&cfdb,query);
   }

/* Associations */

for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {
   if (tp->comment)
      {
      snprintf(longname,CF_BUFSIZE,"%s (%s)",tp->comment,tp->topic_name);
      }
   else
      {
      snprintf(longname,CF_BUFSIZE,"%s",tp->topic_name);
      }

   for (ta = tp->associations; ta != NULL; ta=ta->next)
      {
      for (rp = ta->associates; rp != NULL; rp=rp->next)
         {
         snprintf(query,CF_BUFSIZE-1,"INSERT INTO associations (from_name,to_name,from_assoc,to_assoc,from_type,to_type) values ('%s','%s','%s','%s','%s','%s');\n",longname,GetLongTopicName(TOPIC_MAP,rp->item),ta->fwd_name,ta->bwd_name,tp->topic_type,ta->associate_topic_type);
         fprintf(fout,query);
         CfVoidQueryDB(&cfdb,query);
         }
      }
   }

/* Occurrences */

for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {
   if (tp->comment)
      {
      snprintf(longname,CF_BUFSIZE,"%s (%s)",tp->comment,tp->topic_name);
      }
   else
      {
      snprintf(longname,CF_BUFSIZE,"%s",tp->topic_name);
      }
      
   for (op = tp->occurrences; op != NULL; op=op->next)
      {
      for (rp = op->represents; rp != NULL; rp=rp->next)
         {
         char safeexpr[CF_BUFSIZE];
         strcpy(safeexpr,EscapeSQL(&cfdb,op->locator));
         
         snprintf(query,CF_BUFSIZE-1,"INSERT INTO occurrences (topic_name,locator,locator_type,subtype) values ('%s','%s','%d','%s')\n",longname,safeexpr,op->rep_type,GetLongTopicName(TOPIC_MAP,rp->item));
         fprintf(fout,query);
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

void ShowTopicSummary(CfdbConn *cfdb,char *topic_name,char *topic_type)

{
if (HTML)
   {
   printf("<li>Topic \"%s\" found ",NextTopic(topic_name));
   printf("in the context of \"%s\"\n",NextTopic(topic_type));
   }
else
   {
   printf("Topic \"%s\" found in the context of \"%s\"\n",topic_name,topic_type);
   }
}

/*********************************************************************/

void ShowTopicCosmology(CfdbConn *cfdb,char *this_name,char *this_type)

{ struct Topic *other_topics = NULL;
  struct TopicAssociation *associations = NULL;
  struct Occurrence *occurrences = NULL;
  char topic_name[CF_MAXVARSIZE],topic_id[CF_MAXVARSIZE],topic_type[CF_MAXVARSIZE],associate[CF_MAXVARSIZE];
  char query[CF_MAXVARSIZE],fassociation[CF_MAXVARSIZE],bassociation[CF_MAXVARSIZE];
  char locator[CF_MAXVARSIZE],subtype[CF_MAXVARSIZE];
  enum representations locator_type;
  struct Rlist *rp;

/* Collect data - first other topics of same type */

snprintf(query,CF_BUFSIZE,"SELECT * from topics where topic_type='%s'",this_type);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 3)
   {
   CfOut(cf_error,"","The topics database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,3);
   return;
   }

while(CfFetchRow(cfdb))
   {
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_MAXVARSIZE-1);
   strncpy(topic_id,CfFetchColumn(cfdb,1),CF_MAXVARSIZE-1);
   strncpy(topic_type,CfFetchColumn(cfdb,2),CF_MAXVARSIZE-1);

   if (strcmp(topic_name,this_name) == 0 || strcmp(topic_id,this_name) == 0)
      {
      continue;
      }

   AddTopic(&other_topics,topic_name,topic_type);
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
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_MAXVARSIZE-1);
   strncpy(fassociation,CfFetchColumn(cfdb,1),CF_MAXVARSIZE-1);
   strncpy(bassociation,CfFetchColumn(cfdb,2),CF_MAXVARSIZE-1);
   strncpy(associate,CfFetchColumn(cfdb,3),CF_MAXVARSIZE-1);

   AppendRlist(&this,topic_name,CF_SCALAR);
   AddTopicAssociation(&associations,bassociation,NULL,NULL,this,false);
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
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_MAXVARSIZE-1);
   strncpy(fassociation,CfFetchColumn(cfdb,1),CF_MAXVARSIZE-1);
   strncpy(bassociation,CfFetchColumn(cfdb,2),CF_MAXVARSIZE-1);
   strncpy(associate,CfFetchColumn(cfdb,3),CF_MAXVARSIZE-1);

   AppendRlist(&this,associate,CF_SCALAR);
   AddTopicAssociation(&associations,fassociation,NULL,NULL,this,false);
   DeleteRlist(this);
   }

CfDeleteQuery(cfdb);

/* Finally occurrences of the mentioned topic */

snprintf(query,CF_BUFSIZE,"SELECT * from occurrences where topic_name='%s' or subtype='%s' order by locator_type",this_name,this_name);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 4)
   {
   CfOut(cf_error,"","The occurrences database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,4);
   return;
   }

while(CfFetchRow(cfdb))
   {
   struct Rlist *this = NULL;
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_MAXVARSIZE-1);
   strncpy(locator,CfFetchColumn(cfdb,1),CF_MAXVARSIZE-1);
   locator_type = Str2Int(CfFetchColumn(cfdb,2));
   strncpy(subtype,CfFetchColumn(cfdb,3),CF_MAXVARSIZE-1);

   AppendRlist(&this,subtype,CF_SCALAR);
   AddOccurrence(&occurrences,topic_name,locator,this,locator_type);
   DeleteRlist(this);
   }

CfDeleteQuery(cfdb);

/* Now print the results */

if (HTML)
   {
   ShowHtmlResults(this_name,this_type,other_topics,associations,occurrences);
   }
else
   {
   ShowTextResults(this_name,this_type,other_topics,associations,occurrences);
   }
}

/*********************************************************************/

void ShowAssociationSummary(CfdbConn *cfdb,char *this_fassoc,char *this_tassoc)

{
if (HTML)
   {
   printf("<li>Association \"%s\" ",NextTopic(this_fassoc));
   printf("(inverse name \"%s\")\n",NextTopic(this_tassoc));
   }
else
   {
   printf("Association \"%s\" (inverse name \"%s\")\n",this_fassoc,this_tassoc);
   }
}

/*********************************************************************/

void ShowAssociationCosmology(CfdbConn *cfdb,char *this_fassoc,char *this_tassoc,char *from_type,char *to_type)

{ char topic_name[CF_MAXVARSIZE],topic_id[CF_MAXVARSIZE],topic_type[CF_MAXVARSIZE],query[CF_MAXVARSIZE];

if (HTML)
   {
   CfHtmlHeader(stdout,this_tassoc,STYLESHEET);
   printf("<h1>Association \"%s\" </h1>",NextTopic(this_fassoc));
   printf("with inverse \"%s\", ",NextTopic(this_tassoc));
   printf("associates type \"%s\" ->",NextTopic(from_type));
   printf("&rarr; type \"%s\"\n",NextTopic(to_type));
   }
else
   {
   printf("Association \"%s\" (with inverse \"%s\"), ",this_fassoc,this_tassoc);
   printf("associates type \"%s\" -> type \"%s\"\n",from_type,to_type);
   }


if (HTML)
   {
   printf("\n<h2>Role players</h2>\n\n");
   printf("  <h3>%s::</h3>\n",from_type);
   printf("<div id=\"roles\">\n");
   printf("<ul>\n");
   }
else
   {
   printf("\nRole players\n\n");
   printf("  %s::\n",from_type);
   }

/* Collect data - first other topics of same type (associated role players) */

snprintf(query,CF_BUFSIZE,"SELECT * from topics where topic_type='%s'",from_type);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 3)
   {
   CfOut(cf_error,"","The topics database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,3);
   return;
   }

while(CfFetchRow(cfdb))
   {
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_MAXVARSIZE-1);
   strncpy(topic_id,CfFetchColumn(cfdb,1),CF_MAXVARSIZE-1);
   strncpy(topic_type,CfFetchColumn(cfdb,2),CF_MAXVARSIZE-1);

   if (HTML)
      {
      printf("<li>%s</li>\n",NextTopic(topic_name));
      }
   else
      {
      printf("    %s\n",topic_name);
      }
   }

CfDeleteQuery(cfdb);

if (HTML)
   {
   printf("</ul>\n</div>");
   printf("  <h3>%s::</h3>\n",to_type);
   printf("<div id=\"roles\">\n");
   printf("<ul>\n");
   }
else
   {
   printf("\n  %s::\n",to_type);
   }

/* Collect data - first other topics of same type (associated role players) */

snprintf(query,CF_BUFSIZE,"SELECT * from topics where topic_type='%s'",to_type);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 3)
   {
   CfOut(cf_error,"","The topics database table did not promise the expected number of fields - got %d expected %d\n",cfdb->maxcolumns,3);
   return;
   }

while(CfFetchRow(cfdb))
   {
   strncpy(topic_name,CfFetchColumn(cfdb,0),CF_MAXVARSIZE-1);
   strncpy(topic_id,CfFetchColumn(cfdb,1),CF_MAXVARSIZE-1);
   strncpy(topic_type,CfFetchColumn(cfdb,2),CF_MAXVARSIZE-1);

   if (HTML)
      {
      printf("<li>%s</li>\n",NextTopic(topic_name));
      }
   else
      {
      printf("    %s\n",topic_name);
      }
   }

CfDeleteQuery(cfdb);

if (HTML)
   {
   printf("</ul>\n</div>");
   CfHtmlFooter(stdout);
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

void ShowTextResults(char *this_name,char *this_type,struct Topic *other_topics,struct TopicAssociation *associations,struct Occurrence *occurrences)

{ struct Topic *tp;
  struct TopicAssociation *ta;
  struct Occurrence *oc;
  struct Rlist *rp;
  int count = 0;

printf("\nTopic \"%s\" found in the context of \"%s\"\n",this_name,this_type);
  
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

CfOut(cf_error,"","\nOccurrences of this topic:\n\n");

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
          printf(" %s (URL)",oc->locator);
          break;
      case cfk_file:
          printf(" %s (file)",oc->locator);
          break;
      case cfk_db:
          printf(" %s (DB)",oc->locator);
          break;          
      case cfk_literal:
          printf(" \"%s\" (Quote)",oc->locator);
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
   printf("  %s in the context of %s\n",tp->topic_name,tp->topic_type);
   count++;
   }

if (count == 0)
   {
   printf("\n    (none)\n");
   }
}

/*********************************************************************/

void ShowHtmlResults(char *this_name,char *this_type,struct Topic *other_topics,struct TopicAssociation *associations,struct Occurrence *occurrences)

{ struct Topic *tp;
  struct TopicAssociation *ta;
  struct Occurrence *oc;
  struct Rlist *rp;
  int count = 0;
  FILE *fout = stdout;
  
CfHtmlHeader(stdout,this_name,STYLESHEET);

fprintf(fout,"<div id=\"topic\">");
fprintf(fout,"Topic \"%s\" found in the context of ",NextTopic(this_name));
fprintf(fout,"\"%s\"\n</div>",NextTopic(this_type));

fprintf(fout,"<p><div id=\"associations\">");
fprintf(fout,"\n<h2>Associations:</h2>\n\n");

fprintf(fout,"<ul>\n");

for (ta = associations; ta != NULL; ta=ta->next)
   {
   fprintf(fout,"<li>  %s \"%s\"\n",this_name,NextTopic(ta->fwd_name));

   fprintf(fout,"<ul>\n");
   for (rp = ta->associates; rp != NULL; rp=rp->next)
      {
      fprintf(fout,"<li> %s\n",NextTopic(rp->item));
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

count = 0;

fprintf(fout,"<div id=\"occurrences\">");

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
      for (rp = oc->represents; rp != NULL; rp=rp->next)
         {
         fprintf(fout,"<li> %s: ",NextTopic((char *)rp->item));
         }
      }
   switch (oc->rep_type)
      {
      case cfk_url:
          fprintf(fout," <a href=\"%s\">%s</a> (URL)",oc->locator,oc->locator);
          break;
      case cfk_file:
          fprintf(fout," <a href=\"file://%s\">%s</a> (file)",oc->locator,oc->locator);
          break;
      case cfk_db:
          fprintf(fout," %s (DB)",oc->locator);
          break;          
      case cfk_literal:
          fprintf(fout," \"%s\" (Quote)",oc->locator);
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
count = 0;

fprintf(fout,"<div id=\"others\">\n");

fprintf(fout,"\n<h2>Other topics of the same type (%s):</h2>\n\n",this_type);

fprintf(fout,"<ul>\n");

for (tp = other_topics; tp != NULL; tp=tp->next)
   {
   fprintf(fout,"<li>  %s \n",NextTopic(tp->topic_name));
   fprintf(fout,"in the context of %s\n",NextTopic(tp->topic_type));
   count++;
   }

if (count == 0)
   {
   fprintf(fout,"<li>    (none)\n");
   }

fprintf(fout,"</div>");

fprintf(fout,"</ul>\n");

CfHtmlFooter(stdout);
}

/*********************************************************************/

char *NextTopic(char *topic)

{ static char url[CF_BUFSIZE];

if (strlen(WEBDRIVER) == 0)
   {
   CfOut(cf_error,"","No query_engine is defined\n");
   exit(1);
   }
 
snprintf(url,CF_BUFSIZE,"<a href=\"%s?next=%s\">%s</a>",WEBDRIVER,topic,topic);
return url;
}

/* EOF */




