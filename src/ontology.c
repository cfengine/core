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

/*********************************************************************/

void DeClassifyTopic(char *classified_topic, char *topic, char *context)
{
    context[0] = '\0';
    topic[0] = '\0';

    if (classified_topic == NULL)
    {
        return;
    }

    if (*classified_topic == ':')
    {
        sscanf(classified_topic, "::%255[^\n]", topic);
    }
    else if (strstr(classified_topic, "::"))
    {
        sscanf(classified_topic, "%255[^:]::%255[^\n]", context, topic);

        if (strlen(topic) == 0)
        {
            sscanf(classified_topic, "::%255[^\n]", topic);
        }
    }
    else
    {
        strncpy(topic, classified_topic, CF_MAXVARSIZE - 1);
    }

    if (strlen(context) == 0)
    {
        strcpy(context, "any");
    }
}

/*****************************************************************************/

Topic *GetTopic(Topic *list, char *topic_name)
{
    Topic *tp;
    char context[CF_MAXVARSIZE], name[CF_MAXVARSIZE];

    strncpy(context, topic_name, CF_MAXVARSIZE - 1);
    name[0] = '\0';

    DeClassifyTopic(topic_name, name, context);

    for (tp = list; tp != NULL; tp = tp->next)
    {
        if (strlen(context) == 0)
        {
            if (strcmp(topic_name, tp->topic_name) == 0)
            {
                return tp;
            }
        }
        else
        {
            if (((strcmp(name, tp->topic_name)) == 0) && (strcmp(context, tp->topic_context) == 0))
            {
                return tp;
            }
        }
    }

    return NULL;
}
