#include "symbol_table.h"

#include "map.h"
#include "hashes.h"
#include "string_lib.h"
#include "rlist.h"

typedef struct
{
    Map *entries;
} SymbolTableScope;

typedef struct
{
    Map *scopes;
} SymbolTableNamespace;

struct SymbolTable_
{
    Map *ns;
};

static SymbolTableEntry *SymbolTableEntryNew(const char *lval, Rval rval, DataType type)
{
    SymbolTableEntry *entry = xmalloc(sizeof(SymbolTableEntry));

    entry->lval = xstrdup(lval);
    entry->rval = rval;
    entry->type = type;

    return entry;
}

static void SymbolTableEntryDestroy(SymbolTableEntry *table_entry)
{
    if (table_entry)
    {
        free(table_entry->lval);
        RvalDestroy(table_entry->rval);
        free(table_entry);
    }
}

static SymbolTableScope *SymbolTableScopeNew(void)
{
    SymbolTableScope *table_scope = xmalloc(sizeof(SymbolTableScope));

    table_scope->entries = MapNew((MapHashFn)&OatHash, (MapKeyEqualFn)&StringSafeEqual, &free, (MapDestroyDataFn)SymbolTableEntryDestroy);

    return table_scope;
}

static void SymbolTableScopeDestroy(SymbolTableScope *table_scope)
{
    if (table_scope)
    {
        MapDestroy(table_scope->entries);
        free(table_scope);
    }
}

static SymbolTableNamespace *SymbolTableNamespaceNew(void)
{
    SymbolTableNamespace *table_ns = xmalloc(sizeof(SymbolTableNamespace));

    table_ns->scopes = MapNew((MapHashFn)&OatHash, (MapKeyEqualFn)&StringSafeEqual, &free, (MapDestroyDataFn)SymbolTableScopeDestroy);

    return table_ns;
}

static void SymbolTableNamespaceDestroy(SymbolTableNamespace *table_ns)
{
    if (table_ns)
    {
        MapDestroy(table_ns->scopes);
        free(table_ns);
    }
}

SymbolTable *SymbolTableNew(void)
{
    SymbolTable *table = xmalloc(sizeof(SymbolTable));

    table->ns = MapNew((MapHashFn)&OatHash, (MapKeyEqualFn)&StringSafeEqual, &free, (MapDestroyDataFn)SymbolTableNamespaceDestroy);

    return table;
}

void SymbolTableDestroy(SymbolTable *table)
{
    if (table)
    {
        MapDestroy(table->ns);
        free(table);
    }
}

void SymbolTableAdd(SymbolTable *table, const char *ns, const char *scope, const char *lval, Rval rval, DataType type)
{
    SymbolTableNamespace *table_ns = MapGet(table->ns, ns);
    if (!table_ns)
    {
        table_ns = SymbolTableNamespaceNew();
        MapInsert(table->ns, xstrdup(ns), table_ns);
    }

    SymbolTableScope *table_scope = MapGet(table_ns->scopes, scope);
    if (!table_scope)
    {
        table_scope = SymbolTableScopeNew();
        MapInsert(table_ns->scopes, xstrdup(scope), table_scope);
    }

    MapInsert(table_scope->entries, xstrdup(lval), SymbolTableEntryNew(lval, rval, type));
}

const SymbolTableEntry *SymbolTableGet(const SymbolTable *table, const char *ns, const char *scope, const char *lval)
{
    SymbolTableNamespace *table_ns = MapGet(table->ns, ns);
    if (!table_ns)
    {
        return NULL;
    }

    SymbolTableScope *table_scope = MapGet(table_ns->scopes, scope);
    if (!table_scope)
    {
        return NULL;
    }

    return MapGet(table_scope->entries, lval);
}
