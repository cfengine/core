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

#ifndef CFENGINE_CFKEYFUNCTIONS_H
#define CFENGINE_CFKEYFUNCTIONS_H

#include "generic_agent.h"

#include "lastseen.h"
#include "dir.h"
#include "scope.h"
#include "files_copy.h"
#include "files_interfaces.h"
#include "files_hashes.h"
#include "keyring.h"
#include "env_context.h"
#include "crypto.h"

#ifdef HAVE_NOVA
#include "license.h"
#endif

RSA* LoadPublicKey(const char* filename);
char* GetPubkeyDigest(const char* pubkey);
int PrintDigest(const char* pubkey);
int TrustKey(const char* pubkey);
bool ShowHost(const char *hostkey, const char *address, bool incoming, const KeyHostSeen *quality, void *ctx);
void ShowLastSeenHosts();
int RemoveKeys(const char *host);
void KeepKeyPromises(const char *public_key_file, const char *private_key_file);

bool LicenseInstall(char *path_source);


#endif // CFKEYFUNCTIONS_H
