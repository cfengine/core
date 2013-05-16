/*
   Copyright (C) CFEngine AS

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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_CONVERSION_H
#define CFENGINE_CONVERSION_H

#include "cf3.defs.h"

// Type-String conversion
MeasurePolicy MeasurePolicyFromString(const char *s);
EnvironmentState EnvironmentStateFromString(const char *s);
InsertMatchType InsertMatchTypeFromString(const char *s);
Interval IntervalFromString(const char *s);
DatabaseType DatabaseTypeFromString(const char *s);
ContextScope ContextScopeFromString(const char *scope_str);
FileComparator FileComparatorFromString(const char *s);
FileLinkType FileLinkTypeFromString(const char *s);
DataType DataTypeFromString(const char *name);
const char *DataTypeToString(DataType dtype);
PackageActionPolicy PackageActionPolicyFromString(const char *s);
PackageVersionComparator PackageVersionComparatorFromString(const char *s);
PackageAction PackageActionFromString(const char *s);
AclMethod AclMethodFromString(const char *string);
AclType AclTypeFromString(const char *string);
AclDefault AclDefaultFromString(const char *string);
AclInherit AclInheritFromString(const char *string);
ServicePolicy ServicePolicyFromString(const char *string);
int SignalFromString(const char *s);
int SyslogPriorityFromString(const char *s);
ShellType ShellTypeFromString(const char *s);

// Date/Time conversion
long Months2Seconds(int m);
int Day2Number(const char *datestring);
void TimeToDateStr(time_t t, char *outStr, int outStrSz);
int Month2Int(const char *string);
long TimeAbs2Int(const char *s);


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
void CommandPrefix(char *execstr, char *comm);
const char *DataTypeShortToType(char *short_type);
int FindTypeInArray(const char **haystack, const char *needle, int default_value, int null_value);

UidList *Rlist2UidList(Rlist *uidnames, const Promise *pp);
GidList *Rlist2GidList(Rlist *gidnames, const Promise *pp);
#ifndef __MINGW32__
uid_t Str2Uid(char *uidbuff, char *copy, const Promise *pp);
gid_t Str2Gid(char *gidbuff, char *copy, const Promise *pp);
#endif /* !__MINGW32__ */

#ifdef HAVE_NOVA

const char *Nova_LongArch(const char *arch);
const char *Nova_ShortArch(const char *arch);
int Nova_CoarseLaterThan(const char *key, const char *from);
bool BundleQualifiedNameSplit(const char *qualified_bundle_name, char namespace_out[CF_MAXVARSIZE], char bundle_name_out[CF_MAXVARSIZE]);

/* Timestamp-functions are not standardised across SQL databases - provide a standard layer for simple functions */
char *SqlVariableExpand(const char *query);
#endif



#endif
