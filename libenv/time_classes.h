/*
  Copyright 2024 Northern.tech AS

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

#ifndef CFENGINE_TIME_CLASSES_H
#define CFENGINE_TIME_CLASSES_H

#include <cf3.defs.h>

/**
 * @brief Get a string set of time classes based on a timestamp.
 * @param time Unix timestamp
 * @return Set of time classes or NULL in case of error.
 * @note On success, returned value must be free'd using StringSetDestroy().
 * @example GetTimeClasses(1716989621) in the GMT+2 timezone would return the
 *          following classes: { 'Afternoon', 'Day29', 'GMT_Afternoon',
 *          'GMT_Day29', 'GMT_Hr13', 'GMT_Hr13_Q3', 'GMT_Lcycle_2', 'GMT_May',
 *          'GMT_Min30_35', 'GMT_Min33', 'GMT_Q3', 'GMT_Wednesday',
 *          'GMT_Yr2024', 'Hr15', 'Hr15_Q3', 'Lcycle_2', 'May', 'Min30_35',
 *          'Min33', 'Q3', 'Wednesday', 'Yr2024' }
 */
StringMap *GetTimeClasses(time_t time);

void UpdateTimeClasses(EvalContext *ctx, time_t t);

#endif
