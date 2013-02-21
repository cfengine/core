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

Attributes GetClassContextAttributes(const Promise *pp);
Attributes GetColumnAttributes(const Promise *pp);
Attributes GetDatabaseAttributes(const Promise *pp);
Attributes GetDeletionAttributes(const Promise *pp);
Attributes GetEnvironmentsAttributes(const Promise *pp);
Attributes GetExecAttributes(const Promise *pp);
Attributes GetFilesAttributes(const Promise *pp);
Attributes GetInferencesAttributes(const Promise *pp);
Attributes GetInsertionAttributes(const Promise *pp);
Attributes GetInterfacesAttributes(const Promise *pp);
Attributes GetMeasurementAttributes(const Promise *pp);
Attributes GetMethodAttributes(const Promise *pp);
Attributes GetOccurrenceAttributes(const Promise *pp);
Attributes GetOutputsAttributes(const Promise *pp);
Attributes GetPackageAttributes(const Promise *pp);
Attributes GetProcessAttributes(const Promise *pp);
Attributes GetReplaceAttributes(const Promise *pp);
Attributes GetReportsAttributes(const Promise *pp);
Attributes GetServicesAttributes(const Promise *pp);
Attributes GetStorageAttributes(const Promise *pp);
Attributes GetTopicsAttributes(const Promise *pp);

Acl GetAclConstraints(const Promise *pp);
ContextConstraint GetContextConstraints(const Promise *pp);
Database GetDatabaseConstraints(const Promise *pp);
DefineClasses GetClassDefinitionConstraints(const Promise *pp);
EditColumn GetColumnConstraints(const Promise *pp);
EditDefaults GetEditDefaults(const Promise *pp);
EditLocation GetLocationAttributes(const Promise *pp);
EditXml GetXmlConstraints(const Promise *pp);
EditRegion GetRegionConstraints(const Promise *pp);
EditReplace GetReplaceConstraints(const Promise *pp);
Environments GetEnvironmentsConstraints(const Promise *pp);
ExecContain GetExecContainConstraints(const Promise *pp);
FileChange GetChangeMgtConstraints(const Promise *pp);
FileCopy GetCopyConstraints(const Promise *pp);
FileDelete GetDeleteConstraints(const Promise *pp);
FileLink GetLinkConstraints(const Promise *pp);
FilePerms GetPermissionConstraints(const Promise *pp);
FileRename GetRenameConstraints(const Promise *pp);
FileSelect GetSelectConstraints(const Promise *pp);
LineSelect GetDeleteSelectConstraints(const Promise *pp);
LineSelect GetInsertSelectConstraints(const Promise *pp);
Measurement GetMeasurementConstraint(const Promise *pp);
Packages GetPackageConstraints(const Promise *pp);
ProcessCount GetMatchesConstraints(const Promise *pp);
ProcessSelect GetProcessFilterConstraints(const Promise *pp);
Recursion GetRecursionConstraints(const Promise *pp);
Report GetReportConstraints(const Promise *pp);
Services GetServicesConstraints(const Promise *pp);
StorageMount GetMountConstraints(const Promise *pp);
StorageVolume GetVolumeConstraints(const Promise *pp);
TcpIp GetTCPIPAttributes(const Promise *pp);
TransactionContext GetTransactionConstraints(const Promise *pp);

/* Default values for attributes */

void SetChecksumUpdates(bool enabled);

#endif
