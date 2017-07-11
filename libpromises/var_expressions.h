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

#ifndef CFENGINE_VAR_EXPRESSIONS_H
#define CFENGINE_VAR_EXPRESSIONS_H

#include <platform.h>

#include <string_expressions.h>
#include <policy.h>

/**
   VarRef is immutable, which means that after allocated the members never
   change, until all of it is freed.

   @TODO constify all pointers returned from VarRef initializers (VarRefCopy,
         VarRefParse, etc)
*/
typedef struct
{
    char *ns;
    char *scope;
    char *lval;
    char **indices;
    size_t num_indices;

    /* TODO performance: when using VarRefCopy() we just need to allocate one
     *      big chunk with malloc() and all pointers can point in there.  This
     *      whole struct can be a single malloc'ed chunk with array[] space in
     *      the end, if our use allows it (no changes after creation). */
    /* bool single_alloc; */
    /* char space[];      */
} VarRef;

VarRef *VarRefCopy(const VarRef *ref);
VarRef *VarRefCopyLocalized(const VarRef *ref);
VarRef *VarRefCopyIndexless(const VarRef *ref);

VarRef *VarRefParse(const char *var_ref_string);

VarRef *VarRefParseFromBundle(const char *var_ref_string, const Bundle *bundle);
VarRef *VarRefParseFromScope(const char *var_ref_string, const char *scope);
VarRef *VarRefParseFromNamespaceAndScope(const char *qualified_name,
                                         const char *_ns, const char *_scope,
                                         char ns_separator, char scope_separator);
VarRef VarRefConst(const char *ns, const char *scope, const char *lval);

void VarRefDestroy        (VarRef *ref);
void VarRefDestroy_untyped(void   *ref);

char *VarRefToString(const VarRef *ref, bool qualified);

void VarRefSetMeta(VarRef *ref, bool enabled);

bool VarRefIsQualified(const VarRef *ref);
void VarRefQualify(VarRef *ref, const char *ns, const char *scope);
void VarRefAddIndex(VarRef *ref, const char *index);

int VarRefCompare(const VarRef *a, const VarRef *b);

bool VarRefEqual_untyped(const void *a, const void *b);

unsigned int VarRefHash_untyped(const void *ref,
                                unsigned int seed,
                                unsigned int max);


#endif
