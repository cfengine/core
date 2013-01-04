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
    {"forward_relationship", cf_str, "", "Name of forward association between promiser topic and associates"},
    {"backward_relationship", cf_str, "", "Name of backward/inverse association from associates to promiser topic"},
    {"associates", cf_slist, "", "List of associated topics by this forward relationship"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_OCCUR_BODIES[] =
{
    {"about_topics", cf_slist, "",
     "List of topics that the document or resource addresses"},    
    {"represents", cf_slist, "",
     "List of explanations for what relationship this document has to the topics it is about"},    
    {"representation", cf_opts, "literal,url,db,file,web,image,portal",
     "How to interpret the promiser string e.g. actual data or reference to data"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_TOPICS_BODIES[] =
{
    {"association", cf_body, CF_RELATE_BODY, "Declare associated topics"},
    {"synonyms", cf_slist, "", "A list of words to be treated as equivalents in the defined context"},
    {"generalizations", cf_slist, "",
     "A list of words to be treated as super-sets for the current topic, used when reasoning"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_THING_BODIES[] =
{
    {"synonyms", cf_slist, "", "A list of words to be treated as equivalents in the defined context"},
    {"affects", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"belongs_to", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"causes", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"certainty", cf_opts, "certain,uncertain,possible",
     "Selects the level of certainty for the proposed knowledge, for use in inferential reasoning"},
    {"determines", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"generalizations", cf_slist, "",
     "A list of words to be treated as super-sets for the current topic, used when reasoning"},
    {"implements", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"involves", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"is_caused_by", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"is_connected_to", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"is_determined_by", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"is_followed_by", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"is_implemented_by", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"is_located_in", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"is_measured_by", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"is_part_of", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"is_preceded_by", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"measures", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"needs", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"provides", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {"uses", cf_slist, "", "Special fixed relation for describing topics that are things"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_INFER_BODIES[] =
{
    {"precedents", cf_slist, "", "The foundational vector for a trinary inference"},
    {"qualifiers", cf_slist, "", "The second vector in a trinary inference"},
    {NULL, cf_notype, NULL, NULL}
};

const SubTypeSyntax CF_KNOWLEDGE_SUBTYPES[] =
{
    {"knowledge", "inferences", CF_INFER_BODIES},
    {"knowledge", "things", CF_THING_BODIES},
    {"knowledge", "topics", CF_TOPICS_BODIES},
    {"knowledge", "occurrences", CF_OCCUR_BODIES},
    {NULL, NULL, NULL},
};
