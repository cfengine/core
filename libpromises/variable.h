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
