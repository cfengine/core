/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

#include <cf3.defs.h>

/***************************************************************/

enum modestate
{
    wild,
    who,
    which
};

enum modesort
{
    unknown,
    numeric,
    symbolic
};

/*******************************************************************/

static int CheckModeState(enum modestate stateA, enum modestate stateB, enum modesort modeA, enum modesort modeB,
                          char ch);
static int SetModeMask(char action, int value, int affected, mode_t *p, mode_t *m);

/***************************************************************/

int ParseModeString(const char *modestring, mode_t *plusmask, mode_t *minusmask)
{
    int affected = 0, value = 0, gotaction, no_error = true;
    char action = '=';
    enum modestate state = wild;
    enum modesort found_sort = unknown; /* Already found "sort" of mode */
    enum modesort sort = unknown;       /* Sort of started but not yet finished mode */

    *plusmask = *minusmask = 0;

    if (modestring == NULL)
    {
        return true;
    }

    gotaction = false;

    for (const char *sp = modestring; true; sp++)
    {
        switch (*sp)
        {
        case 'a':
            no_error = CheckModeState(who, state, symbolic, sort, *sp);
            affected |= 07777;
            sort = symbolic;
            break;

        case 'u':
            no_error = CheckModeState(who, state, symbolic, sort, *sp);
            affected |= 04700;
            sort = symbolic;
            break;

        case 'g':
            no_error = CheckModeState(who, state, symbolic, sort, *sp);
            affected |= 02070;
            sort = symbolic;
            break;

        case 'o':
            no_error = CheckModeState(who, state, symbolic, sort, *sp);
            affected |= 00007;
            sort = symbolic;
            break;

        case '+':
        case '-':
        case '=':
            if (gotaction)
            {
                Log(LOG_LEVEL_ERR, "Too many +-= in mode string");
                return false;
            }

            no_error = CheckModeState(who, state, symbolic, sort, *sp);
            action = *sp;
            state = which;
            gotaction = true;
            sort = unknown;
            break;

        case 'r':
            no_error = CheckModeState(which, state, symbolic, sort, *sp);
            value |= 0444 & affected;
            sort = symbolic;
            break;

        case 'w':
            no_error = CheckModeState(which, state, symbolic, sort, *sp);
            value |= 0222 & affected;
            sort = symbolic;
            break;

        case 'x':
            no_error = CheckModeState(which, state, symbolic, sort, *sp);
            value |= 0111 & affected;
            sort = symbolic;
            break;

        case 's':
            no_error = CheckModeState(which, state, symbolic, sort, *sp);
            value |= 06000 & affected;
            sort = symbolic;
            break;

        case 't':
            no_error = CheckModeState(which, state, symbolic, sort, *sp);
            value |= 01000;
            sort = symbolic;
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
            no_error = CheckModeState(which, state, numeric, sort, *sp);
            sort = numeric;
            gotaction = true;
            state = which;
            affected = 07777;   /* TODO: Hard-coded; see below */
            sscanf(sp, "%o", &value);

            if (value & S_IFMT)
            {
                Log(LOG_LEVEL_INFO, "Mode-Value is not entirely within the system's allowed permissions (octal %o) and will be filtered accordingly : %s",
                    S_IFMT, modestring);
            }

            /* stat() returns the file types in the mode, but they
             * can't be set.  So we clear the file-type as per POSIX
             * 2001 instead of erroring out, leaving just the
             * permissions. */
            value &= ~S_IFMT;
            
            if (value > 07777)  /* TODO: Hardcoded !
                                   Is this correct for all sorts of Unix ?
                                   What about NT ?
                                   Any (POSIX)-constants ??
                                 */
            {
                Log(LOG_LEVEL_ERR, "Mode-Value too big : %s", modestring);
                return false;
            }

            while ((isdigit((int) *sp)) && (*sp != '\0'))
            {
                sp++;
            }
            sp--;
            break;

        case ',':
            if (!SetModeMask(action, value, affected, plusmask, minusmask))
            {
                return false;
            }

            if ((found_sort != unknown) && (found_sort != sort))
            {
                Log(LOG_LEVEL_INFO, "Symbolic and numeric form for modes mixed");
            }

            found_sort = sort;
            sort = unknown;
            action = '=';
            affected = 0;
            value = 0;
            gotaction = false;
            state = who;
            break;

        case '\0':
            if ((state == who) || (value == 0))
            {
                if ((strcmp(modestring, "0000") != 0) && (strcmp(modestring, "000") != 0))
                {
                    Log(LOG_LEVEL_ERR, "mode string is incomplete");
                    return false;
                }
            }

            if (!SetModeMask(action, value, affected, plusmask, minusmask))
            {
                return false;
            }

            if ((found_sort != unknown) && (found_sort != sort))
            {
                Log(LOG_LEVEL_INFO, "Symbolic and numeric form for modes mixed");
            }

            Log(LOG_LEVEL_DEBUG,
                "Modestring [PLUS = %jo] [MINUS = %jo]",
                (uintmax_t) *plusmask, (uintmax_t) *minusmask);
            return true;

        default:
            Log(LOG_LEVEL_ERR, "Invalid mode string (%s)", modestring);
            return false;
        }
    }

    if (!no_error)
    {
        Log(LOG_LEVEL_ERR, "Error validating mode string %s", modestring);
    }

    return no_error;
}

/*********************************************************/

static int CheckModeState(enum modestate stateA, enum modestate stateB, enum modesort sortA, enum modesort sortB,
                          char ch)
{
    if ((stateA != wild) && (stateB != wild) && (stateA != stateB))
    {
        Log(LOG_LEVEL_ERR, "Mode string constant (%c) used out of context", ch);
        return false;
    }

    if ((sortA != unknown) && (sortB != unknown) && (sortA != sortB))
    {
        Log(LOG_LEVEL_ERR, "Symbolic and numeric filemodes mixed within expression");
        return false;
    }

    return true;
}

/*********************************************************/

static int SetModeMask(char action, int value, int affected, mode_t *p, mode_t *m)
{
    switch (action)
    {
    case '+':
        *p |= value;
        *m |= 0;
        return true;
    case '-':
        *p |= 0;
        *m |= value;
        return true;
    case '=':
        *p |= value;
        *m |= ((~value) & 07777 & affected);
        return true;
    default:
        Log(LOG_LEVEL_ERR, "Mode directive %c is unknown", action);
        return false;
    }
}
