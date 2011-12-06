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

#ifndef CFENGINE_ONTOLOGY_H
#define CFENGINE_ONTOLOGY_H

void AddInference(struct Inference **list,char *result,char *pre,char *qual);
struct Topic *IdempInsertTopic(char *classified_name);
struct Topic *InsertTopic(char *name,char *context);
struct Topic *FindTopic(char *name);
int GetTopicPid(char *typed_topic);
struct Topic *AddTopic(struct Topic **list,char *name,char *type);
void AddTopicAssociation(struct Topic *tp,struct TopicAssociation **list,char *fwd_name,char *bwd_name,struct Rlist *li,int ok,char *from_context,char *from_topic);
void AddOccurrence(struct Occurrence **list,char *reference,struct Rlist *represents,enum representations rtype,char *context);
struct Topic *TopicExists(char *topic_name,char *topic_type);
struct Topic *GetCanonizedTopic(struct Topic *list,char *topic_name);
struct Topic *GetTopic(struct Topic *list,char *topic_name);
struct TopicAssociation *AssociationExists(struct TopicAssociation *list,char *fwd,char *bwd);
struct Occurrence *OccurrenceExists(struct Occurrence *list,char *locator,enum representations repy_type,char *s);
void DeClassifyTopic(char *typdetopic,char *topic,char *type);

#endif
