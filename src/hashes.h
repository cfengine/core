#ifndef CFENGINE_HASHES_H
#define CFENGINE_HASHES_H

#include "cf3.defs.h"

#include "assoc.h"

/* - specific hashes - */

int RefHash(char *name);
int ElfHash(char *key);
int OatHash(const char *key);
int GetHash(const char *name);

/* - hashtable operations - */

AssocHashTable *HashInit(void);

/* Insert element if it does not exist in hash table. Returns false if element
   already exists in table or if table is full. */
bool HashInsertElement(AssocHashTable *hashtable, const char *element, Rval rval, enum cfdatatype dtype);

/* Deletes element from hashtable, returning whether element was found */
bool HashDeleteElement(AssocHashTable *hashtable, const char *element);

/* Looks up element in hashtable, returns NULL if not found */
CfAssoc *HashLookupElement(AssocHashTable *hashtable, const char *element);

/* Copies all elements of old hash table to new one. */
void HashCopy(AssocHashTable *newhash, AssocHashTable *oldhash);

/* Clear whole hash table */
void HashClear(AssocHashTable *hashtable);

/* Destroy hash table */
void HashFree(AssocHashTable *hashtable);

/* HashToList */
void HashToList(Scope *sp, Rlist **list);

/* - hashtable iterator - */

/*
HashIterator i = HashIteratorInit(hashtable);
CfAssoc *assoc;
while ((assoc = HashIteratorNext(&i)))
   {
   // do something with assoc;
   }
// No cleanup is required
*/
HashIterator HashIteratorInit(AssocHashTable *hashtable);
CfAssoc *HashIteratorNext(HashIterator *iterator);

#endif
