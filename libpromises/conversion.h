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

#ifndef CFENGINE_CONVERSION_H
#define CFENGINE_CONVERSION_H

#include <cf3.defs.h>

// Type-String conversion
MeasurePolicy MeasurePolicyFromString(const char *s);
EnvironmentState EnvironmentStateFromString(const char *s);
InsertMatchType InsertMatchTypeFromString(const char *s);
Interval IntervalFromString(const char *s);
DatabaseType DatabaseTypeFromString(const char *s);
UserState UserStateFromString(const char *s);
PasswordFormat PasswordFormatFromString(const char *s);
ContextScope ContextScopeFromString(const char *scope_str);
FileComparator FileComparatorFromString(const char *s);
FileLinkType FileLinkTypeFromString(const char *s);
DataType DataTypeFromString(const char *name);
const char *DataTypeToString(DataType dtype);
PackageActionPolicy PackageActionPolicyFromString(const char *s);
PackageVersionComparator PackageVersionComparatorFromString(const char *s);
PackageAction PackageActionFromString(const char *s);
NewPackageAction GetNewPackagePolicy(const char *s, const char **action_types);
AclMethod AclMethodFromString(const char *string);
AclType AclTypeFromString(const char *string);
AclDefault AclDefaultFromString(const char *string);
AclInherit AclInheritFromString(const char *string);
int SignalFromString(const char *s);
int SyslogPriorityFromString(const char *s);
ShellType ShellTypeFromString(const char *s);

// Date/Time conversion
void TimeToDateStr(time_t t, char *outStr, int outStrSz);
int Month2Int(const char *string);

// Evalaution conversion
bool BooleanFromString(const char *val);
long IntFromString(const char *s);
bool DoubleFromString(const char *s, double *value_out);
bool IntegerRangeFromString(const char *intrange, long *min_out, long *max_out);
bool IsRealNumber(const char *s);


// Misc.
char *Rlist2String(Rlist *list, char *sep); // TODO: Yet another Rlist serialization scheme.. Found 5 so far.
DataType ConstraintSyntaxGetDataType(const ConstraintSyntax *body_syntax, const char *lval);
const char *MapAddress(const char *addr);
const char *CommandArg0(const char *execstr);
size_t CommandArg0_bound(char *dst, const char *src, size_t dst_size);
void CommandPrefix(char *execstr, char *comm);
const char *DataTypeShortToType(char *short_type);
bool DataTypeIsIterable(DataType t);

int CoarseLaterThan(const char *key, const char *from);
int FindTypeInArray(const char *const haystack[], const char *needle, int default_value, int null_value);

void UidListDestroy(UidList *uids);
void GidListDestroy(GidList *gids);
UidList *Rlist2UidList(Rlist *uidnames, const Promise *pp);
GidList *Rlist2GidList(Rlist *gidnames, const Promise *pp);
#ifndef __MINGW32__
uid_t Str2Uid(const char *uidbuff, char *copy, const Promise *pp);
gid_t Str2Gid(const char *gidbuff, char *copy, const Promise *pp);
#endif /* !__MINGW32__ */

#endif
