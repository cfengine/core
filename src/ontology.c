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
/* File: ontology.c                                                          */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

int GLOBAL_ID = 1; // Used as a primary key for convenience, 0 reserved
extern struct Occurrences *OCCURRENCES;

/*****************************************************************************/

struct Topic *IdempInsertTopic(char *classified_name)

{ char context[CF_MAXVARSIZE],topic[CF_MAXVARSIZE];
   
context[0] = '\0';
topic[0] = '\0';

DeClassifyTopic(classified_name,topic,context);

return InsertTopic(topic,context);
}

/*****************************************************************************/

struct Topic *InsertTopic(char *name,char *context)

{ int slot = GetHash(ToLowerStr(name));

return AddTopic(&(TOPICHASH[slot]),name,context);
}

/*****************************************************************************/

struct Topic *FindTopic(char *name)

{ int slot = GetHash(ToLowerStr(name));

return GetTopic(TOPICHASH[slot],name);
}

/*****************************************************************************/

struct Topic *AddTopic(struct Topic **list,char *name,char *context)

{ struct Topic *tp;

if (tp = TopicExists(name,context))
   {
   CfOut(cf_verbose,""," -> Topic %s already defined, ok\n",name);
   }
else
   {
   if ((tp = (struct Topic *)malloc(sizeof(struct Topic))) == NULL)
      {
      CfOut(cf_error,"malloc"," !! Memory failure in AddTopic");
      FatalError("");
      }

   tp->topic_name = strdup(NormalizeTopic(name));
   
   if (context && strlen(context) > 0)
      {
      tp->topic_context = strdup(NormalizeTopic(context));
      }
   else
      {
      tp->topic_context = strdup("any");
      }

   tp->id = GLOBAL_ID++;
   tp->associations = NULL;
   tp->synonyms = NULL;
   tp->next = *list;
   *list = tp;
   CF_NODES++;
   }

return tp;
}

/*****************************************************************************/

void AddTopicAssociation(struct TopicAssociation **list,char *fwd_name,char *bwd_name,struct Rlist *associates,int verify)

{ struct TopicAssociation *ta = NULL,*texist;
  char fwd_context[CF_MAXVARSIZE];
  struct Rlist *rp;

strncpy(fwd_context,CanonifyName(fwd_name),CF_MAXVARSIZE-1);

if (associates == NULL || associates->item == NULL)
   {
   CfOut(cf_error," !! A topic must have at least one associate in association %s",fwd_name);
   return;
   }

if ((texist = AssociationExists(*list,fwd_name,bwd_name,verify)) == NULL)
   {
   if ((ta = (struct TopicAssociation *)malloc(sizeof(struct TopicAssociation))) == NULL)
      {
      CfOut(cf_error,"malloc","Memory failure in AddTopicAssociation");
      FatalError("");
      }
   
   if ((ta->fwd_name = strdup(fwd_name)) == NULL)
      {
      CfOut(cf_error,"malloc","Memory failure in AddTopicAssociation");
      FatalError("");
      }

   ta->bwd_name = NULL;
       
   if (bwd_name && ((ta->bwd_name = strdup(bwd_name)) == NULL))
      {
      CfOut(cf_error,"malloc","Memory failure in AddTopicAssociation");
      FatalError("");
      }
   
   if (fwd_context && (ta->fwd_context = strdup(fwd_context)) == NULL)
      {
      CfOut(cf_error,"malloc","Memory failure in AddTopicAssociation");
      FatalError("");
      }

   ta->associates = NULL;
   ta->bwd_context = NULL;
   ta->next = *list;
   *list = ta;
   }
else
   {
   ta = texist;
   }

/* Association now exists, so add new members */

for (rp = associates; rp != NULL; rp=rp->next)
   {
   IdempPrependRScalar(&(ta->associates),NormalizeTopic(rp->item),rp->type);
   CfOut(cf_verbose,""," --> Adding associate '%s'",rp->item);
   IdempInsertTopic(rp->item);
   CF_EDGES++;
   }
}

/*****************************************************************************/

void AddOccurrence(struct Occurrence **list,char *reference,struct Rlist *represents,enum representations rtype,char *context)

{ struct Occurrence *op = NULL;
  struct TopRepresentation *tr;
  struct Rlist *rp;

  if ((op = OccurrenceExists(*list,reference,rtype,context)) == NULL)
   {
   if ((op = (struct Occurrence *)malloc(sizeof(struct Occurrence))) == NULL)
      {
      CfOut(cf_error,"malloc","Memory failure in AddOccurrence");
      FatalError("");
      }

   op->represents = NULL;
   op->occurrence_context = strdup(context);
   op->locator = strdup(reference);
   op->rep_type = rtype;   
   op->next = *list;
   *list = op;
   CF_EDGES++;
   CF_NODES++;
   CfOut(cf_verbose,""," -> Noted occurrence for %s::%s",context,reference);
   }

/* Occurrence now exists, so add new subtype promises */

if (represents == NULL)
   {
   CfOut(cf_error,""," !! Topic occurrence \"%s\" claims to represent no aspect of its topic, discarding...",reference);
   return;
   }

for (rp = represents; rp != NULL; rp=rp->next)
   {
   IdempPrependRScalar(&(op->represents),rp->item,rp->type);
   }
}

/*********************************************************************/

char *ClassifiedTopic(char *topic,char *context)

{ static char name[CF_MAXVARSIZE];

Debug("CONTEXT(%s)/TOPIC(%s)",context,topic);

if (context && strlen(context) > 0)
   {
   snprintf(name,CF_MAXVARSIZE,"%s::%s",context,topic);
   }
else
   {
   snprintf(name,CF_MAXVARSIZE,"%s",topic);
   }

return name;
}

/*********************************************************************/

void DeClassifyTopic(char *classified_topic,char *topic,char *context)

{
context[0] = '\0';
topic[0] = '\0';

if (classified_topic == NULL)
   {
   return;
   }

if (*classified_topic == ':')
   {
   sscanf(classified_topic,"::%255[^\n]",topic);
   }
else if (strstr(classified_topic,"::"))
   {
   sscanf(classified_topic,"%255[^:]::%255[^\n]",context,topic);
   
   if (strlen(topic) == 0)
      {
      sscanf(classified_topic,"::%255[^\n]",topic);
      }
   }
else
   {
   strncpy(topic,classified_topic,CF_MAXVARSIZE-1);
   }
}

/*********************************************************************/

void DeClassifyCanonicalTopic(char *classified_topic,char *topic,char *context)

{
context[0] = '\0';
topic[0] = '\0';

if (*classified_topic == '.')
   {
   sscanf(classified_topic,".%255[^\n]",topic);
   }
else if (strstr(classified_topic,"."))
   {
   sscanf(classified_topic,"%255[^.].%255[^\n]",context,topic);
   
   if (strlen(topic) == 0)
      {
      sscanf(classified_topic,".%255[^\n]",topic);
      }
   }
else
   {
   strncpy(topic,classified_topic,CF_MAXVARSIZE-1);
   }
}

/*********************************************************************/

int ClassifiedTopicMatch(char *ttopic1,char *ttopic2)

{ char context1[CF_MAXVARSIZE],topic1[CF_MAXVARSIZE];
  char context2[CF_MAXVARSIZE],topic2[CF_MAXVARSIZE];

if (strcmp(ttopic1,ttopic2) == 0)
   {
   return true;
   }
   
context1[0] = '\0';
topic1[0] = '\0';
context2[0] = '\0';
topic2[0] = '\0';

DeClassifyTopic(ttopic1,topic1,context1);
DeClassifyTopic(ttopic2,topic2,context2);

if (strlen(context1) > 0 && strlen(context2) > 0)
   {
   if (strcmp(topic1,topic2) == 0 && strcmp(context1,context2) == 0)
      {
      return true;
      }
   }
else
   {
   if (strcmp(topic1,topic2) == 0)
      {
      return true;
      }
   }

return false;
}

/*****************************************************************************/

int GetTopicPid(char *classified_topic)

{ struct Topic *tp;
  int slot;
  char context[CF_MAXVARSIZE],name[CF_MAXVARSIZE];

name[0] = '\0';

DeClassifyTopic(classified_topic,name,context);
slot = GetHash(ToLowerStr(name));

if (tp = GetTopic(TOPICHASH[slot],classified_topic))
   {
   return tp->id;
   }

return 0;
}

/*****************************************************************************/

char *URLHint(char *url)

{ char *sp;

for (sp = url+strlen(url); *sp != '/'; sp--)
   {
   }

return sp;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

struct Topic *TopicExists(char *topic_name,char *topic_context)

{ struct Topic *tp;
  char c[CF_BUFSIZE];
  int slot;

slot = GetHash(ToLowerStr(topic_name));
  
for (tp = TOPICHASH[slot]; tp != NULL; tp=tp->next)
   {
   if (strcmp(tp->topic_name,NormalizeTopic(topic_name)) == 0)
      {
      if (topic_context)
         {
         if (strlen(topic_context) > 0 && strcmp(tp->topic_context,NormalizeTopic(topic_context)) == 0)
            {
            return tp;
            }

         if (strlen(topic_context) == 0 && strcmp(tp->topic_context,"any") == 0)
            {
            return tp;
            }
         }
      }
   }

return NULL;
}

/*****************************************************************************/

struct TopicAssociation *AssociationExists(struct TopicAssociation *list,char *fwd,char *bwd,int verify)

{ struct TopicAssociation *ta;
  int yfwd = false,ybwd = false;
  enum cfreport level;
  char l[CF_BUFSIZE],r[CF_BUFSIZE];

if (verify)
   {
   level = cf_error;
   }
else
   {
   level = cf_verbose;
   }

if (fwd == NULL || (fwd && strlen(fwd) == 0))
   {
   CfOut(cf_error,"","NULL forward association name\n");
   return NULL;
   }

if (bwd == NULL || (bwd && strlen(bwd) == 0))
   {
   CfOut(cf_verbose,"","NULL backward association name\n");
   }

for (ta = list; ta != NULL; ta=ta->next)
   {
   if (fwd && (strcmp(fwd,ta->fwd_name) == 0))
      {
      CfOut(cf_verbose,"","Association %s exists already\n",fwd);
      yfwd = true;
      }
   else if (fwd && ta->fwd_name)
      {
      strncpy(l,ToLowerStr(fwd),CF_MAXVARSIZE);
      strncpy(r,ToLowerStr(ta->fwd_name),CF_MAXVARSIZE);
      
      if (strcmp(l,r) == 0)
         {
         CfOut(cf_error,""," ! Association \"%s\" exists with different capitalization \"%s\"\n",fwd,ta->fwd_name);
         yfwd = true;
         }
      else
         {
         yfwd = false;
         }
      }
   else
      {
      yfwd = false;
      }

   if (bwd && (strcmp(bwd,ta->bwd_name) == 0))
      {
      CfOut(cf_verbose,""," ! Association %s exists already\n",bwd);
      ybwd = true;
      }
   else if (bwd && ta->bwd_name)
      {
      strncpy(l,ToLowerStr(bwd),CF_MAXVARSIZE);
      strncpy(r,ToLowerStr(ta->bwd_name),CF_MAXVARSIZE);
      
      if (strcmp(l,r) == 0)
         {
         CfOut(cf_inform,""," ! Association \"%s\" exists with different capitalization \"%s\"\n",bwd,ta->bwd_name);
         }

      ybwd = true;
      }
   else if (!bwd && ta->bwd_name == NULL)
      {
      ybwd = true;
      }
   else
      {
      ybwd = false;
      }
   
   if (ta->bwd_name && (strcmp(fwd,ta->bwd_name) == 0) && bwd && (strcmp(bwd,ta->fwd_name) == 0))
      {
      CfOut(cf_inform,""," ! Association \"%s\" exists already but in opposite orientation\n",fwd);
      return ta;
      }

   if (yfwd && ybwd)
      {
      return ta;
      }
   }

return NULL;
}

/*****************************************************************************/

struct Occurrence *OccurrenceExists(struct Occurrence *list,char *locator,enum representations rep_type,char *context)

{ struct Occurrence *op;
  
for (op = list; op != NULL; op=op->next)
   {
   if (strcmp(locator,op->locator) == 0 && strcmp(op->occurrence_context,context) == 0)
      {
      return op;
      }
   }

return NULL;
}

/*****************************************************************************/

struct Topic *GetTopic(struct Topic *list,char *topic_name)

{ struct Topic *tp;
  char context[CF_MAXVARSIZE],name[CF_MAXVARSIZE],*sp;

strncpy(context,topic_name,CF_MAXVARSIZE-1);
name[0] = '\0';

DeClassifyTopic(topic_name,name,context);

for (tp = list; tp != NULL; tp=tp->next)
   {
   if (strlen(context) == 0)
      {
      if (strcmp(topic_name,tp->topic_name) == 0)
         {
         return tp;          
         }
      }
   else
      {
      if ((strcmp(name,tp->topic_name)) == 0 && (strcmp(context,tp->topic_context) == 0))
         {
         return tp;          
         }
      }
   }

return NULL;
}

/*****************************************************************************/

struct Topic *GetCanonizedTopic(struct Topic *list,char *topic_name)

{ struct Topic *tp;
  char context[CF_MAXVARSIZE],name[CF_MAXVARSIZE],*sp;

DeClassifyCanonicalTopic(topic_name,name,context);

for (tp = list; tp != NULL; tp=tp->next)
   {
   if (strlen(context) == 0)
      {
      if (strcmp(name,CanonifyName(tp->topic_name)) == 0)
         {
         return tp;          
         }
      }
   else
      {
      if ((strcmp(name,CanonifyName(tp->topic_name))) == 0 && (strcmp(context,CanonifyName(tp->topic_context)) == 0))
         {
         return tp;          
         }
      }
   }

return NULL;
}

/*****************************************************************************/

char *GetTopicContext(char *topic_name)

{ struct Topic *tp;
  static char context1[CF_MAXVARSIZE],topic1[CF_MAXVARSIZE];
  int slot = GetHash(topic_name);
 
context1[0] = '\0';
DeClassifyTopic(topic_name,topic1,context1);

if (strlen(context1) > 0)
   {
   return context1;
   }

for (tp = TOPICHASH[slot]; tp != NULL; tp=tp->next)
   {
   if (strcmp(topic1,tp->topic_name) == 0)
      {
      return tp->topic_context;
      }
   }

return NULL;
}

/*****************************************************************************/

char *NormalizeTopic(char *s)

{ char *sp;
  int special = false;

for (sp = s; *sp != '\0'; sp++)
   {
   if (IsIn(*sp,"/\\&|=$@"))
      {
      special = true;
      break;
      }
   }

if (special)
   {
   return s;
   }
else
   {
   return ToLowerStr(s);
   }
}
