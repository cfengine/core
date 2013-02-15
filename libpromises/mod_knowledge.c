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
#include "mod_knowledge.h"

static const BodySyntax CF_RELATE_BODY[] =
{
    {"forward_relationship", DATA_TYPE_STRING, "", "Name of forward association between promiser topic and associates"},
    {"backward_relationship", DATA_TYPE_STRING, "", "Name of backward/inverse association from associates to promiser topic"},
    {"associates", DATA_TYPE_STRING_LIST, "", "List of associated topics by this forward relationship"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

static const BodySyntax CF_OCCUR_BODIES[] =
{
    {"about_topics", DATA_TYPE_STRING_LIST, "",
     "List of topics that the document or resource addresses"},    
    {"represents", DATA_TYPE_STRING_LIST, "",
     "List of explanations for what relationship this document has to the topics it is about"},    
    {"representation", DATA_TYPE_OPTION, "literal,url,db,file,web,image,portal",
     "How to interpret the promiser string e.g. actual data or reference to data"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

static const BodySyntax CF_TOPICS_BODIES[] =
{
    {"association", DATA_TYPE_BODY, CF_RELATE_BODY, "Declare associated topics"},
    {"synonyms", DATA_TYPE_STRING_LIST, "", "A list of words to be treated as equivalents in the defined context"},
    {"generalizations", DATA_TYPE_STRING_LIST, "",
     "A list of words to be treated as super-sets for the current topic, used when reasoning"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

static const BodySyntax CF_THING_BODIES[] =
{
    {"synonyms", DATA_TYPE_STRING_LIST, "", "A list of words to be treated as equivalents in the defined context"},
    {"affects", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"belongs_to", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"causes", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"certainty", DATA_TYPE_OPTION, "certain,uncertain,possible",
     "Selects the level of certainty for the proposed knowledge, for use in inferential reasoning"},
    {"determines", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"generalizations", DATA_TYPE_STRING_LIST, "",
     "A list of words to be treated as super-sets for the current topic, used when reasoning"},
    {"implements", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"involves", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"is_caused_by", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"is_connected_to", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"is_determined_by", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"is_followed_by", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"is_implemented_by", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"is_located_in", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"is_measured_by", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"is_part_of", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"is_preceded_by", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"measures", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"needs", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"provides", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {"uses", DATA_TYPE_STRING_LIST, "", "Special fixed relation for describing topics that are things"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

static const BodySyntax CF_INFER_BODIES[] =
{
    {"precedents", DATA_TYPE_STRING_LIST, "", "The foundational vector for a trinary inference"},
    {"qualifiers", DATA_TYPE_STRING_LIST, "", "The second vector in a trinary inference"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const SubTypeSyntax CF_KNOWLEDGE_SUBTYPES[] =
{
    {"knowledge", "inferences", CF_INFER_BODIES},
    {"knowledge", "things", CF_THING_BODIES},
    {"knowledge", "topics", CF_TOPICS_BODIES},
    {"knowledge", "occurrences", CF_OCCUR_BODIES},
    {NULL, NULL, NULL},
};
