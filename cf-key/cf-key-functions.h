/*
   Copyright 2018 Northern.tech AS

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

#ifndef CFENGINE_CFKEYFUNCTIONS_H
#define CFENGINE_CFKEYFUNCTIONS_H

#include <generic_agent.h>

#include <openssl/rsa.h>

#include <lastseen.h>
#include <dir.h>
#include <scope.h>
#include <files_copy.h>
#include <files_interfaces.h>
#include <files_hashes.h>
#include <keyring.h>
#include <eval_context.h>
#include <crypto.h>


extern bool LOOKUP_HOSTS;

int PrintDigest(const char *pubkey);
void ParseKeyArg(char *keyarg, char **filename, char **ipaddr, char **username);
void ShowLastSeenHosts(bool truncate);
int RemoveKeys(const char *input, bool must_be_coherent);
bool KeepKeyPromises(const char *public_key_file, const char *private_key_file, const int key_size);
int ForceKeyRemoval(const char *key);
int ForceIpAddressRemoval(const char *ip);

#endif // CFKEYFUNCTIONS_H
