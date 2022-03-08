/*
  Copyright 2021 Northern.tech AS

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

#ifndef CFENGINE_UNIX_H
#define CFENGINE_UNIX_H

#include <cf3.defs.h>
#include <logging.h>

void ProcessSignalTerminate(pid_t pid);
bool GetCurrentUserName(char *userName, int userNameLen);

#ifndef __MINGW32__
/**
 * Get user name for the user with UID #uid
 *
 * @param uid              UID of the user
 * @param user_name_buf    buffer to store the user name (if found)
 * @param buf_size         size of the #user_name_buf buffer
 * @param error_log_level  log level to store errors with (not found or an actual error)
 * @return                 whether the lookup was successful or not
 */
bool GetUserName(uid_t uid, char *user_name_buf, size_t buf_size, LogLevel error_log_level);
bool GetGroupName(gid_t gid, char *group_name_buf, size_t buf_size, LogLevel error_log_level);

/**
 * Get UID for the user with user name #user_name
 *
 * @param user_name        user name of the user
 * @param[out] uid         place to store the UID of user #user_name (if found)
 * @param error_log_level  log level to store errors with (not found or an actual error)
 * @return                 whether the lookup was successful or not
 */
bool GetUserID(const char *user_name, uid_t *uid, LogLevel error_log_level);
bool GetGroupID(const char *group_name, gid_t *gid, LogLevel error_log_level);
#endif

#endif
