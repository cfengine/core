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

#ifndef CFENGINE_MOD_COMMON_H
#define CFENGINE_MOD_COMMON_H

#include <cf3.defs.h>


typedef enum
{
    SERVER_CONTROL_ALLOW_ALL_CONNECTS,
    SERVER_CONTROL_ALLOW_CONNECTS,
    SERVER_CONTROL_ALLOW_USERS,
    SERVER_CONTROL_AUDITING,
    SERVER_CONTROL_BIND_TO_INTERFACE,
    SERVER_CONTROL_CFRUNCOMMAND,
    SERVER_CONTROL_CALL_COLLECT_INTERVAL,
    SERVER_CONTROL_CALL_COLLECT_WINDOW,
    SERVER_CONTROL_DENY_BAD_CLOCKS,
    SERVER_CONTROL_DENY_CONNECTS,
    SERVER_CONTROL_DYNAMIC_ADDRESSES,
    SERVER_CONTROL_HOSTNAME_KEYS,
    SERVER_CONTROL_KEY_TTL,
    SERVER_CONTROL_LOG_ALL_CONNECTIONS,
    SERVER_CONTROL_LOG_ENCRYPTED_TRANSFERS,
    SERVER_CONTROL_MAX_CONNECTIONS,
    SERVER_CONTROL_PORT_NUMBER,
    SERVER_CONTROL_SERVER_FACILITY,
    SERVER_CONTROL_SKIP_VERIFY,
    SERVER_CONTROL_TRUST_KEYS_FROM,
    SERVER_CONTROL_LISTEN,
    SERVER_CONTROL_ALLOWCIPHERS,
    SERVER_CONTROL_ALLOWLEGACYCONNECTS,
    SERVER_CONTROL_ALLOWTLSVERSION,
    SERVER_CONTROL_MAX
} ServerControl;


extern const ConstraintSyntax CFS_CONTROLBODY[SERVER_CONTROL_MAX + 1];


CommonControl CommonControlFromString(const char *lval);


#endif
