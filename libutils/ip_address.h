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

#ifndef CFENGINE_IP_ADDRESS_H
#define CFENGINE_IP_ADDRESS_H

#include "buffer.h"

typedef struct IPAddress IPAddress;
typedef enum
{
    IP_ADDRESS_TYPE_IPV4,
    IP_ADDRESS_TYPE_IPV6
} IPAddressVersion;

/**
  @brief Creates a new IPAddress object from a string.
  @param source Buffer containing the string representation of the ip address.
  @return A fully formed IPAddress object or NULL if there was an error parsing the source.
  */
IPAddress *IPAddressNew(Buffer *source);
/**
  @brief Destroys an IPAddress object.
  @param address IPAddress object to be destroyed.
  */
int IPAddressDestroy(IPAddress **address);
/**
  @brief Returns the type of address.
  @param address Address object.
  @return The type of address or -1 in case of error.
  */
int IPAddressType(IPAddress *address);
/**
  @brief Produces a fully usable IPV6 or IPV4 address string representation.
  @param address IPAddress object.
  @return A buffer containing an IPV4 or IPV6 address or NULL in case the given address was invalid.
  */
Buffer *IPAddressGetAddress(IPAddress *address);
/**
  @brief Recovers the appropriate port from the given address.
  @param address IPAddress object.
  @return A valid port for connections or -1 if it was not available.
  */
int IPAddressGetPort(IPAddress *address);
#endif // CFENGINE_IP_ADDRESS_H
