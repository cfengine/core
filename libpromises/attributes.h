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

#ifndef CFENGINE_ATTRIBUTES_H
#define CFENGINE_ATTRIBUTES_H

#include <cf3.defs.h>

Attributes GetClassContextAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetColumnAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetDatabaseAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetDeletionAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetEnvironmentsAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetExecAttributes(const EvalContext *ctx, const Promise *pp);
void ClearFilesAttributes(Attributes *whom);
/* Every return from GetFilesAttributes() must be passed to
 * ClearFilesAttributes() when you're done with it. */
Attributes GetFilesAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetInferencesAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetInsertionAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetMeasurementAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetMethodAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetOccurrenceAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetPackageAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetUserAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetProcessAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetReplaceAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetReportsAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetServicesAttributes(const EvalContext *ctx, const Promise *pp);
Attributes GetStorageAttributes(const EvalContext *ctx, const Promise *pp);

Acl GetAclConstraints(const EvalContext *ctx, const Promise *pp);
ContextConstraint GetContextConstraints(const EvalContext *ctx, const Promise *pp);
Database GetDatabaseConstraints(const EvalContext *ctx, const Promise *pp);
DefineClasses GetClassDefinitionConstraints(const EvalContext *ctx, const Promise *pp);
EditColumn GetColumnConstraints(const EvalContext *ctx, const Promise *pp);
EditDefaults GetEditDefaults(const EvalContext *ctx, const Promise *pp);
EditLocation GetLocationAttributes(const Promise *pp);
EditXml GetXmlConstraints(const Promise *pp);
EditRegion GetRegionConstraints(const EvalContext *ctx, const Promise *pp);
EditReplace GetReplaceConstraints(const Promise *pp);
Environments GetEnvironmentsConstraints(const EvalContext *ctx, const Promise *pp);
ExecContain GetExecContainConstraints(const EvalContext *ctx, const Promise *pp);
ENTERPRISE_FUNC_0ARG_DECLARE(HashMethod, GetBestFileChangeHashMethod);
FileChange GetChangeMgtConstraints(const EvalContext *ctx, const Promise *pp);
FileCopy GetCopyConstraints(const EvalContext *ctx, const Promise *pp);
FileDelete GetDeleteConstraints(const EvalContext *ctx, const Promise *pp);
FileLink GetLinkConstraints(const EvalContext *ctx, const Promise *pp);
FileRename GetRenameConstraints(const EvalContext *ctx, const Promise *pp);
FileSelect GetSelectConstraints(const EvalContext *ctx, const Promise *pp);
LineSelect GetDeleteSelectConstraints(const EvalContext *ctx, const Promise *pp);
LineSelect GetInsertSelectConstraints(const EvalContext *ctx, const Promise *pp);
Measurement GetMeasurementConstraint(const EvalContext *ctx, const Promise *pp);
Packages GetPackageConstraints(const EvalContext *ctx, const Promise *pp);
NewPackages GetNewPackageConstraints(const EvalContext *ctx, const Promise *pp);
ProcessCount GetMatchesConstraints(const EvalContext *ctx, const Promise *pp);
ProcessSelect GetProcessFilterConstraints(const EvalContext *ctx, const Promise *pp);
DirectoryRecursion GetRecursionConstraints(const EvalContext *ctx, const Promise *pp);
Report GetReportConstraints(const EvalContext *ctx, const Promise *pp);
Services GetServicesConstraints(const EvalContext *ctx, const Promise *pp);
StorageMount GetMountConstraints(const EvalContext *ctx, const Promise *pp);
StorageVolume GetVolumeConstraints(const EvalContext *ctx, const Promise *pp);
TransactionContext GetTransactionConstraints(const EvalContext *ctx, const Promise *pp);

#endif
