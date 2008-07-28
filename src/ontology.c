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
/* File: ontology.c                                                          */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void AddTopic(struct Topic **list,char *name,char *type)

{ struct Topic *tp;

if (TopicExists(*list,name,type))
   {
   Verbose("Topic %s already defined\n",name);
   return;
   }
 
if ((tp = (struct Topic *)malloc(sizeof(struct Topic))) == NULL)
   {
   CfOut(cferror,"malloc","Memory failure in AddTopic");
   FatalError("");
   }

if ((tp->topic_name = strdup(name)) == NULL)
   {
   CfOut(cferror,"malloc","Memory failure in AddTopic");
   FatalError("");
   }

if ((tp->topic_type = strdup(type)) == NULL)
   {
   CfOut(cferror,"malloc","Memory failure in AddTopic");
   FatalError("");
   }

tp->comment = NULL;
tp->associations = NULL;
tp->next = *list;
*list = tp;
}


/*****************************************************************************/

void AddCommentedTopic(struct Topic **list,char *name,char *comment,char *type)

{ struct Topic *tp;

if (TopicExists(*list,name,type))
   {
   Verbose("Topic %s already defined\n",name);
   return;
   }
 
if ((tp = (struct Topic *)malloc(sizeof(struct Topic))) == NULL)
   {
   CfOut(cferror,"malloc","Memory failure in AddTopic");
   FatalError("");
   }

if ((tp->topic_name = strdup(name)) == NULL)
   {
   CfOut(cferror,"malloc","Memory failure in AddTopic");
   FatalError("");
   }

if ((tp->comment = strdup(comment)) == NULL)
   {
   CfOut(cferror,"malloc","Memory failure in AddTopic");
   FatalError("");
   }

if ((tp->topic_type = strdup(type)) == NULL)
   {
   CfOut(cferror,"malloc","Memory failure in AddTopic");
   FatalError("");
   }

tp->associations = NULL;
tp->next = *list;
*list = tp;
}

/*****************************************************************************/

void AddTopicAssociation(struct TopicAssociation **list,char *fwd_name,char *bwd_name,char *topic_type,struct Rlist *associates)

{ struct TopicAssociation *ta = NULL;
  char assoc_type[CF_MAXVARSIZE];
  struct Rlist *rp;

strncpy(assoc_type,CanonifyName(fwd_name),CF_MAXVARSIZE-1);

if (associates == NULL || associates->item == NULL)
   {
   CfOut(cf_error,"A topic must have at least one associate in association %s",fwd_name);
   return;
   }

if (!AssociationExists(*list,fwd_name,bwd_name))
   {
   if ((ta = (struct TopicAssociation *)malloc(sizeof(struct TopicAssociation))) == NULL)
      {
      CfOut(cferror,"malloc","Memory failure in AddTopicAssociation");
      FatalError("");
      }
   
   if ((ta->fwd_name = strdup(fwd_name)) == NULL)
      {
      CfOut(cferror,"malloc","Memory failure in AddTopicAssociation");
      FatalError("");
      }

   ta->bwd_name = NULL;
       
   if (bwd_name && ((ta->bwd_name = strdup(bwd_name)) == NULL))
      {
      CfOut(cferror,"malloc","Memory failure in AddTopicAssociation");
      FatalError("");
      }
   
   if ((ta->assoc_type = strdup(assoc_type)) == NULL)
      {
      CfOut(cferror,"malloc","Memory failure in AddTopicAssociation");
      FatalError("");
      }

   ta->associate_topic_type = NULL;
   ta->next = *list;
   *list = ta;
   }

/* Association now exists, so add new members */

for (rp = associates; rp != NULL; rp=rp->next)
   {
   /* Defer checking until we have whole ontlogy - all types */
   IdempPrependRScalar(&(ta->associates),rp->item,rp->type);
   }
}

/*****************************************************************************/

void AddOccurrence(struct Occurrence **list,char *topic_name,char *reference,struct Rlist *represents,enum representations rtype)

{ struct Occurrence *op = NULL;
  struct TopRepresentation *tr;
  struct Rlist *rp;

if (!OccurrenceExists(*list,reference,rtype))
   {
   if ((op = (struct Occurrence *)malloc(sizeof(struct TopicAssociation))) == NULL)
      {
      CfOut(cferror,"malloc","Memory failure in AddTopicAssociation");
      FatalError("");
      }

   op->locator = strdup(reference);
   op->rep_type = rtype;   
   op->next = *list;
   *list = op;
   }

/* Occurrence now exists, so add new subtype promises */

if (represents == NULL)
   {
   CfOut(cf_error,"","Class occurrence \"%s\" claims to represent no topics, in which case it is dud",topic_name);
   return;
   }

for (rp = represents; rp != NULL; rp=rp->next)
   {
   IdempPrependRScalar(&(op->represents),rp->item,rp->type);
   }
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int TopicExists(struct Topic *list,char *topic_name,char *topic_type)

{ struct Topic *tp;

for (tp = list; tp != NULL; tp=tp->next)
   {
   if (strcmp(tp->topic_name,topic_name) == 0)
      {
      if (topic_type && strcmp(tp->topic_type,topic_type) == 0)
         {
         CfOut(cf_error,"","Topic \"%s\" exists, but its type \"%s\" does not match promised type \"%s\"",topic_name,tp->topic_type,topic_type);
         }
      return true;
      }
   }

return false;
}

/*****************************************************************************/

int AssociationExists(struct TopicAssociation *list,char *fwd,char *bwd)

{ struct TopicAssociation *ta;
  int yfwd = false,ybwd = false;

for (ta = list; ta != NULL; ta=ta->next)
   {
   if (strcmp(fwd,ta->fwd_name) == 0)
      {
      Verbose("Association exists already");
      yfwd = true;
      }

   if (strcmp(bwd,ta->bwd_name) == 0)
      {
      Verbose("Association exists already");
      yfwd = true;
      }

   if (strcmp(fwd,ta->bwd_name) == 0)
      {
      CfOut(cf_error,"","Association exists already but in opposite orientation");
      return true;
      }

   if (strcmp(bwd,ta->fwd_name) == 0)
      {
      CfOut(cf_error,"","Association exists already but in opposite orientation");
      return true;
      }

   if (yfwd && ybwd)
      {
      return true;
      }
   
   if (yfwd && !ybwd)
      {
      CfOut(cf_error,"","Association %s exists but the reverse association is missing",fwd);
      return true;
      }
   
   if (!yfwd && ybwd)
      {
      CfOut(cf_error,"","The reverse ssociation %s exists but the forward association is missing",fwd);
      return true;
      }

   }

return false;
}

/*****************************************************************************/

int OccurrenceExists(struct Occurrence *list,char *locator,enum representations rep_type)

{

return false;
}

/*****************************************************************************/

struct Topic *GetTopic(struct Topic *list,char *topic_name)

{ struct Topic *tp;

for (tp = list; tp != NULL; tp=tp->next)
   {
   if (strcmp(topic_name,tp->topic_name) == 0)
      {
      return tp;          
      }
   }

return NULL;
}



/*****************************************************************************/

char *GetTopicType(struct Topic *list,char *topic_name)

{ struct Topic *tp;

for (tp = list; tp != NULL; tp=tp->next)
   {
   if (strcmp(topic_name,tp->topic_name) == 0)
      {
      return tp->topic_type;
      }
   }

return NULL;
}
