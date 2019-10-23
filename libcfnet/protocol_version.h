/*
   Copyright 2019 Northern.tech AS

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


#ifndef CFENGINE_PROTOCOL_VERSION_H
#define CFENGINE_PROTOCOL_VERSION_H

/**
  Available protocol versions. When connection is initialised ProtocolVersion
  is 0, i.e. undefined. It is after the call to ServerConnection() that
  protocol version is decided, according to body copy_from and body common
  control. All protocol numbers are numbered incrementally starting from 1.
 */
typedef enum
{
    CF_PROTOCOL_UNDEFINED = 0,
    CF_PROTOCOL_CLASSIC = 1,
    /* --- Greater versions use TLS as secure communications layer --- */
    CF_PROTOCOL_TLS = 2
} ProtocolVersion;

/* We use CF_PROTOCOL_LATEST as the default for new connections. */
#define CF_PROTOCOL_LATEST CF_PROTOCOL_TLS

static inline const char *ProtocolVersionString(const ProtocolVersion p)
{
    switch (p)
    {
    case CF_PROTOCOL_TLS:
        return "tls";
    case CF_PROTOCOL_CLASSIC:
        return "classic";
    default:
        return "undefined";
    }
}

static inline bool ProtocolIsKnown(const ProtocolVersion p)
{
    return ((p > CF_PROTOCOL_UNDEFINED) && (p <= CF_PROTOCOL_LATEST));
}

static inline bool ProtocolIsTLS(const ProtocolVersion p)
{
    return ((p >= CF_PROTOCOL_TLS) && (p <= CF_PROTOCOL_LATEST));
}

static inline bool ProtocolIsTooNew(const ProtocolVersion p)
{
    return (p > CF_PROTOCOL_LATEST);
}

static inline bool ProtocolIsUndefined(const ProtocolVersion p)
{
    return (p <= CF_PROTOCOL_UNDEFINED);
}

static inline bool ProtocolIsClassic(const ProtocolVersion p)
{
    return (p == CF_PROTOCOL_CLASSIC);
}

/**
 * Returns CF_PROTOCOL_TLS or CF_PROTOCOL_CLASSIC (or CF_PROTOCOL_UNDEFINED)
 * Maps all versions using TLS to CF_PROTOCOL_TLS for convenience
 * in switch statements.
 */
static inline ProtocolVersion ProtocolClassicOrTLS(const ProtocolVersion p)
{
    if (ProtocolIsTLS(p))
    {
        return CF_PROTOCOL_TLS;
    }
    if (ProtocolIsClassic(p))
    {
        return CF_PROTOCOL_CLASSIC;
    }
    return CF_PROTOCOL_UNDEFINED;
}

/**
 * Parses the version string sent over network to enum, e.g. "CFE_v1" -> 1
 */
ProtocolVersion ParseProtocolVersionNetwork(const char *s);

/**
 * Parses the version string set in policy to enum, e.g. "classic" -> 1
 */
ProtocolVersion ParseProtocolVersionPolicy(const char *s);

// Old name for compatibility (enterprise), TODO remove:
#define ProtocolVersionParse ParseProtocolVersionPolicy

#endif // CFENGINE_PROTOCOL_VERSION_H
