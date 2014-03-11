#ifndef CFENGINE_CLASS_H
#define CFENGINE_CLASS_H

#include <cf3.defs.h>
#include <set.h>

typedef struct
{
    char *ns;
    char *name;
    size_t hash;

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
