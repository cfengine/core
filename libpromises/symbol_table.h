#ifndef CFENGINE_SYMBOL_TABLE_H
#define CFENGINE_SYMBOL_TABLE_H

#include "cf3.defs.h"

typedef struct
{
    char *lval;
    Rval rval;
    DataType type;
} SymbolTableEntry;

typedef struct SymbolTable_ SymbolTable;

SymbolTable *SymbolTableNew(void);
void SymbolTableDestroy(SymbolTable *table);

void SymbolTableAdd(SymbolTable *table, const char *ns, const char *scope, const char *lval, Rval rval, DataType type);
const SymbolTableEntry *SymbolTableGet(const SymbolTable *table, const char *ns, const char *scope, const char *lval);

#endif
