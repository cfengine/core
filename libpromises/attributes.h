/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_ATTRIBUTES_H
#define CFENGINE_ATTRIBUTES_H

#include "cf3.defs.h"

Attributes GetClassContextAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetColumnAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetDatabaseAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetDeletionAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetEnvironmentsAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetExecAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetFilesAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetInferencesAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetInsertionAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetInterfacesAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetMeasurementAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetMethodAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetOccurrenceAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetOutputsAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetPackageAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetProcessAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetReplaceAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetReportsAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetServicesAttributes(EvalContext *ctx, const Promise *pp);
Attributes GetStorageAttributes(EvalContext *ctx, const Promise *pp);

Acl GetAclConstraints(EvalContext *ctx, const Promise *pp);
ContextConstraint GetContextConstraints(EvalContext *ctx, const Promise *pp);
Database GetDatabaseConstraints(EvalContext *ctx, const Promise *pp);
DefineClasses GetClassDefinitionConstraints(EvalContext *ctx, const Promise *pp);
EditColumn GetColumnConstraints(EvalContext *ctx, const Promise *pp);
EditDefaults GetEditDefaults(EvalContext *ctx, const Promise *pp);
EditLocation GetLocationAttributes(EvalContext *ctx, const Promise *pp);
EditXml GetXmlConstraints(EvalContext *ctx, const Promise *pp);
EditRegion GetRegionConstraints(EvalContext *ctx, const Promise *pp);
EditReplace GetReplaceConstraints(EvalContext *ctx, const Promise *pp);
Environments GetEnvironmentsConstraints(EvalContext *ctx, const Promise *pp);
ExecContain GetExecContainConstraints(EvalContext *ctx, const Promise *pp);
FileChange GetChangeMgtConstraints(EvalContext *ctx, const Promise *pp);
FileCopy GetCopyConstraints(EvalContext *ctx, const Promise *pp);
FileDelete GetDeleteConstraints(EvalContext *ctx, const Promise *pp);
FileLink GetLinkConstraints(EvalContext *ctx, const Promise *pp);
FilePerms GetPermissionConstraints(EvalContext *ctx, const Promise *pp);
FileRename GetRenameConstraints(EvalContext *ctx, const Promise *pp);
FileSelect GetSelectConstraints(EvalContext *ctx, const Promise *pp);
LineSelect GetDeleteSelectConstraints(EvalContext *ctx, const Promise *pp);
LineSelect GetInsertSelectConstraints(EvalContext *ctx, const Promise *pp);
Measurement GetMeasurementConstraint(EvalContext *ctx, const Promise *pp);
Packages GetPackageConstraints(EvalContext *ctx, const Promise *pp);
ProcessCount GetMatchesConstraints(EvalContext *ctx, const Promise *pp);
ProcessSelect GetProcessFilterConstraints(EvalContext *ctx, const Promise *pp);
Recursion GetRecursionConstraints(EvalContext *ctx, const Promise *pp);
Report GetReportConstraints(EvalContext *ctx, const Promise *pp);
Services GetServicesConstraints(EvalContext *ctx, const Promise *pp);
StorageMount GetMountConstraints(EvalContext *ctx, const Promise *pp);
StorageVolume GetVolumeConstraints(EvalContext *ctx, const Promise *pp);
TcpIp GetTCPIPAttributes(EvalContext *ctx, const Promise *pp);
TransactionContext GetTransactionConstraints(EvalContext *ctx, const Promise *pp);

/* Default values for attributes */

void SetChecksumUpdates(bool enabled);

#endif
