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
#ifndef CFENGINE_VARIABLE_H
#define CFENGINE_VARIABLE_H

#include <cf3.defs.h>

#include <var_expressions.h>

typedef struct
{
    VarRef *ref;
    Rval rval;
    DataType type;
    StringSet *tags;
    const Promise *promise; // The promise that set the present value
} Variable;

typedef struct VariableTable_ VariableTable;
typedef struct VariableTableIterator_ VariableTableIterator;

VariableTable *VariableTableNew(void);
void VariableTableDestroy(VariableTable *table);

bool VariableTablePut(VariableTable *table, const VarRef *ref,
                      const Rval *rval, DataType type, const
                      char *tags, const Promise *promise);
Variable *VariableTableGet(const VariableTable *table, const VarRef *ref);
bool VariableTableRemove(VariableTable *table, const VarRef *ref);

size_t VariableTableCount(const VariableTable *table, const char *ns, const char *scope, const char *lval);
bool VariableTableClear(VariableTable *table, const char *ns, const char *scope, const char *lval);

VariableTableIterator *VariableTableIteratorNew(const VariableTable *table, const char *ns, const char *scope, const char *lval);
VariableTableIterator *VariableTableIteratorNewFromVarRef(const VariableTable *table, const VarRef *ref);
Variable *VariableTableIteratorNext(VariableTableIterator *iter);
void VariableTableIteratorDestroy(VariableTableIterator *iter);

VariableTable *VariableTableCopyLocalized(const VariableTable *table, const char *ns, const char *scope);

#endif
