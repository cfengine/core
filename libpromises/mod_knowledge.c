/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

#include <mod_knowledge.h>

#include <syntax.h>

static const ConstraintSyntax association_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewString("forward_relationship", "", "Name of forward association between promiser topic and associates", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("backward_relationship", "", "Name of backward/inverse association from associates to promiser topic", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("associates", "", "List of associated topics by this forward relationship", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewNull()
};

static const BodySyntax association_body = BodySyntaxNew("association", association_constraints, NULL, SYNTAX_STATUS_REMOVED);

static const ConstraintSyntax topics_constraints[] =
{
    ConstraintSyntaxNewBody("association", &association_body, "Declare associated topics", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("synonyms", "", "A list of words to be treated as equivalents in the defined context", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("generalizations", "", "A list of words to be treated as super-sets for the current topic, used when reasoning", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax occurrences_constraints[] =
{
    ConstraintSyntaxNewStringList("about_topics", "", "List of topics that the document or resource addresses", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("represents", "", "List of explanations for what relationship this document has to the topics it is about", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOption("representation", "literal,url,db,file,web,image,portal", "How to interpret the promiser string e.g. actual data or reference to data", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax things_constraints[] =
{
    ConstraintSyntaxNewStringList("synonyms", "", "A list of words to be treated as equivalents in the defined context", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("affects", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("belongs_to", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("causes", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOption("certainty", "certain,uncertain,possible", "Selects the level of certainty for the proposed knowledge, for use in inferential reasoning", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("determines", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("generalizations", "", "A list of words to be treated as super-sets for the current topic, used when reasoning", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("implements", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("involves", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("is_caused_by", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("is_connected_to", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("is_determined_by", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("is_followed_by", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("is_implemented_by", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("is_located_in", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("is_measured_by", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("is_part_of", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("is_preceded_by", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("measures", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("needs", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("provides", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("uses", "", "Special fixed relation for describing topics that are things", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax inferences_constraints[] =
{
    ConstraintSyntaxNewStringList("precedents", "", "The foundational vector for a trinary inference", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("qualifiers", "", "The second vector in a trinary inference", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_KNOWLEDGE_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("knowledge", "inferences", inferences_constraints, NULL, SYNTAX_STATUS_REMOVED),
    PromiseTypeSyntaxNew("knowledge", "things", things_constraints, NULL, SYNTAX_STATUS_REMOVED),
    PromiseTypeSyntaxNew("knowledge", "topics", topics_constraints, NULL, SYNTAX_STATUS_REMOVED),
    PromiseTypeSyntaxNew("knowledge", "occurrences", occurrences_constraints, NULL, SYNTAX_STATUS_REMOVED),
    PromiseTypeSyntaxNewNull()
};
