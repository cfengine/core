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

#ifndef CFENGINE_LASTSEEN_H
#define CFENGINE_LASTSEEN_H

typedef struct
{
    time_t lastseen;
    QPoint Q;
} KeyHostSeen;

typedef enum
{
    LAST_SEEN_ROLE_CONNECT,
    LAST_SEEN_ROLE_ACCEPT
} LastSeenRole;


bool Address2Hostkey(char *dst, size_t dst_size, const char *address);

void LastSaw1(const char *ipaddress, const char *hashstr, LastSeenRole role);
void LastSaw(const char *ipaddress, const char *digest, LastSeenRole role);

bool DeleteIpFromLastSeen(const char *ip, char *digest);
bool DeleteDigestFromLastSeen(const char *key, char *ip);

/*
 * Return false in order to stop iteration
 */
typedef bool (*LastSeenQualityCallback)(const char *hostkey, const char *address,
                                        bool incoming, const KeyHostSeen *quality,
                                        void *ctx);

bool ScanLastSeenQuality(LastSeenQualityCallback callback, void *ctx);
int LastSeenHostKeyCount(void);
bool IsLastSeenCoherent(void);
int RemoveKeysFromLastSeen(const char *input, bool must_be_coherent,
                           char *equivalent);

#endif
