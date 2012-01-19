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

Attributes GetClassContextAttributes(Promise *pp);
Attributes GetColumnAttributes(Promise *pp);
Attributes GetDatabaseAttributes(Promise *pp);
Attributes GetDeletionAttributes(Promise *pp);
Attributes GetEnvironmentsAttributes(Promise *pp);
Attributes GetExecAttributes(Promise *pp);
Attributes GetFilesAttributes(Promise *pp);
Attributes GetInferencesAttributes(Promise *pp);
Attributes GetInsertionAttributes(Promise *pp);
Attributes GetInterfacesAttributes(Promise *pp);
Attributes GetMeasurementAttributes(Promise *pp);
Attributes GetMethodAttributes(Promise *pp);
Attributes GetOccurrenceAttributes(Promise *pp);
Attributes GetOutputsAttributes(Promise *pp);
Attributes GetPackageAttributes(Promise *pp);
Attributes GetProcessAttributes(Promise *pp);
Attributes GetReplaceAttributes(Promise *pp);
Attributes GetReportsAttributes(Promise *pp);
Attributes GetServicesAttributes(Promise *pp);
Attributes GetStorageAttributes(Promise *pp);
Attributes GetThingsAttributes(Promise *pp);
Attributes GetTopicsAttributes(Promise *pp);

Acl GetAclConstraints(Promise *pp);
Context GetContextConstraints(Promise *pp);
Database GetDatabaseConstraints(Promise *pp);
DefineClasses GetClassDefinitionConstraints(Promise *pp);
EditColumn GetColumnConstraints(Promise *pp);
EditDefaults GetEditDefaults(Promise *pp);
EditLocation GetLocationAttributes(Promise *pp);
EditRegion GetRegionConstraints(Promise *pp);
EditReplace GetReplaceConstraints(Promise *pp);
Environments GetEnvironmentsConstraints(Promise *pp);
ExecContain GetExecContainConstraints(Promise *pp);
FileChange GetChangeMgtConstraints(Promise *pp);
FileCopy GetCopyConstraints(Promise *pp);
FileDelete GetDeleteConstraints(Promise *pp);
FileLink GetLinkConstraints(Promise *pp);
FilePerms GetPermissionConstraints(Promise *pp);
FileRename GetRenameConstraints(Promise *pp);
FileSelect GetSelectConstraints(Promise *pp);
LineSelect GetDeleteSelectConstraints(Promise *pp);
LineSelect GetInsertSelectConstraints(Promise *pp);
Measurement GetMeasurementConstraint(Promise *pp);
Packages GetPackageConstraints(Promise *pp);
ProcessCount GetMatchesConstraints(Promise *pp);
ProcessSelect GetProcessFilterConstraints(Promise *pp);
Recursion GetRecursionConstraints(Promise *pp);
Report GetReportConstraints(Promise *pp);
Services GetServicesConstraints(Promise *pp);
StorageMount GetMountConstraints(Promise *pp);
StorageVolume GetVolumeConstraints(Promise *pp);
TcpIp GetTCPIPAttributes(Promise *pp);
TopicAssociation GetAssociationConstraints(Promise *pp);
TransactionContext GetTransactionConstraints(Promise *pp);

/* Default values for attributes */

void SetChecksumUpdates(bool enabled);

#endif
