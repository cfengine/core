/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
#ifndef CFENGINE_CLASS_H
#define CFENGINE_CLASS_H

#include <cf3.defs.h>
#include <set.h>

typedef struct
{
    char *ns;                          /* NULL in case of default namespace */
    char *name;                        /* class name */

    ContextScope scope;
    bool is_soft;
    StringSet *tags;
} Class;


typedef struct ClassTable_ ClassTable;
typedef struct ClassTableIterator_ ClassTableIterator;

ClassTable *ClassTableNew(void);
void ClassTableDestroy(ClassTable *table);

bool ClassTablePut(ClassTable *table, const char *ns, const char *name, bool is_soft, ContextScope scope, const char *tags);
Class *ClassTableGet(const ClassTable *table, const char *ns, const char *name);
Class *ClassTableMatch(const ClassTable *table, const char *regex);
bool ClassTableRemove(ClassTable *table, const char *ns, const char *name);

bool ClassTableClear(ClassTable *table);

ClassTableIterator *ClassTableIteratorNew(const ClassTable *table, const char *ns, bool is_hard, bool is_soft);
Class *ClassTableIteratorNext(ClassTableIterator *iter);
void ClassTableIteratorDestroy(ClassTableIterator *iter);

typedef struct
{
    char *ns;
    char *name;
} ClassRef;

ClassRef ClassRefParse(const char *expr);
char *ClassRefToString(const char *ns, const char *name);
bool ClassRefIsQualified(ClassRef ref);
void ClassRefQualify(ClassRef *ref, const char *ns);
void ClassRefDestroy(ClassRef ref);

#endif
