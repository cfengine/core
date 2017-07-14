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

#include <csv_parser.h>
#include <alloc.h>
#include <writer.h>

typedef enum
{
    CSV_ST_NEW_LINE,
    CSV_ST_PRE_START_SPACE,
    CSV_ST_NO_QUOTE_MODE,
    CSV_ST_SEPARATOR,
    CSV_ST_LEADING_QUOTE,
    CSV_ST_INTERNAL_QUOTE,
    CSV_ST_WITH_QUOTE_MODE,
    CSV_ST_SPACE_AFTER_QUOTE,
    CSV_ST_ERROR,
    CSV_ST_CLOSED
} csv_state;

#define CSVCL_BLANK(x)  (((x)==' ')||((x)=='\t')||((x)=='\n')||((x)=='\r'))
#define CSVCL_QUOTE(x)  (((x)=='"'))
#define CSVCL_SEP(x)    (((x)==','))
#define CSVCL_EOL(x)    (((x)=='\0'))

#define CSVCL_ANY1(x) ((!CSVCL_BLANK(x))&&(!CSVCL_QUOTE(x))&&(!CSVCL_SEP(x)))
#define CSVCL_ANY2(x) ((!CSVCL_BLANK(x))&&(!CSVCL_QUOTE(x))&&(!CSVCL_SEP(x)))
#define CSVCL_ANY3(x) ((!CSVCL_QUOTE(x))&&(!CSVCL_SEP(x)))
#define CSVCL_ANY4(x) ((!CSVCL_BLANK(x))&&(!CSVCL_QUOTE(x))&&(!CSVCL_SEP(x)))
#define CSVCL_ANY5(x) ((!CSVCL_QUOTE(x)))
#define CSVCL_ANY6(x) ((!CSVCL_BLANK(x))&&(!CSVCL_QUOTE(x))&&(!CSVCL_SEP(x)))
#define CSVCL_ANY7(x) ((!CSVCL_QUOTE(x)))
#define CSVCL_ANY8(x) ((!CSVCL_BLANK(x))&&(!CSVCL_SEP(x)))

typedef enum {
    CSV_ERR_OK,
    CVS_ERR_MALFORMED,
    CSV_ERR_UNKNOWN_STATE,
    CSV_ERR_UNEXPECTED_END,
    CSV_ERR_INVALID_INPUT
} csv_parser_error;


/**
 @brief parse CSV-formatted line and put its content in a list

 @param[in] str: is the CSV string to parse
 @param[out] newlist: list of elements found

 @retval 0: successful, <>0: failed
 */
static csv_parser_error LaunchCsvAutomata(const char *str, Seq **newlist)
{
    assert(str);

    if (str == NULL)
    {
        return CSV_ERR_INVALID_INPUT;
    }

    char *snatched = xmalloc(strlen(str) + 1);
    snatched[0] = '\0';
    char *sn = snatched;

    csv_parser_error ret;
    csv_state current_state = CSV_ST_NEW_LINE;             /* initial state */
    const char *s;

    for (s = str;  *s != '\0';  s++)
    {
        switch(current_state) {
            case CSV_ST_ERROR:
                ret = CVS_ERR_MALFORMED;
                goto clean;

            case CSV_ST_NEW_LINE:
                if (CSVCL_SEP(*s))
                {
                    *sn = '\0'; sn = NULL;
                    SeqAppend(*newlist, xstrdup(""));
                    current_state = CSV_ST_SEPARATOR;
                }
                else if (CSVCL_BLANK(*s))
                {
                    *sn = *s; sn++;
                    current_state = CSV_ST_PRE_START_SPACE;
                }
                else if (CSVCL_QUOTE(*s))
                {
                    snatched[0] = '\0'; sn = NULL;
                    current_state = CSV_ST_LEADING_QUOTE;
                }
                else if (CSVCL_ANY1(*s))
                {
                    *sn = *s; sn++;
                    current_state = CSV_ST_NO_QUOTE_MODE;
                }
                break;

            case CSV_ST_PRE_START_SPACE:
                if (CSVCL_SEP(*s))
                {
                    *sn = '\0'; sn = NULL;
                    SeqAppend(*newlist, xstrdup(snatched));
                    snatched[0] = '\0';
                    current_state = CSV_ST_SEPARATOR;
                }
                else if (CSVCL_BLANK(*s))
                {
                    *sn = *s; sn++;
                    current_state = CSV_ST_PRE_START_SPACE;
                }
                else if (CSVCL_QUOTE(*s))
                {
                    snatched[0] = '\0'; sn = NULL;
                    current_state = CSV_ST_LEADING_QUOTE;
                }
                else if (CSVCL_ANY2(*s))
                {
                    *sn = *s; sn++;
                    current_state = CSV_ST_NO_QUOTE_MODE;
                }
                break;

            case CSV_ST_NO_QUOTE_MODE:
                if (CSVCL_SEP(*s))
                {
                    *sn = '\0'; sn = NULL;
                    SeqAppend(*newlist, xstrdup(snatched));
                    snatched[0] = '\0';
                    current_state = CSV_ST_SEPARATOR;
                }
                else if (CSVCL_QUOTE(*s))
                {
                    snatched[0] = '\0'; sn = NULL;
                    current_state = CSV_ST_ERROR;
                }
                else if (CSVCL_ANY3(*s))
                {
                    *sn = *s; sn++;
                    current_state = CSV_ST_NO_QUOTE_MODE;
                }
                break;

            case CSV_ST_SEPARATOR:
                if (CSVCL_SEP(*s))
                {
                    snatched[0] = '\0'; sn = NULL;
                    SeqAppend(*newlist, xstrdup(snatched));
                    current_state = CSV_ST_SEPARATOR;
                }
                else if (CSVCL_BLANK(*s))
                {
                    sn = snatched; *sn = *s; sn++;
                    current_state = CSV_ST_PRE_START_SPACE;
                }
                else if (CSVCL_QUOTE(*s))
                {
                    snatched[0] = '\0'; sn = NULL;
                    current_state = CSV_ST_LEADING_QUOTE;
                }
                else if (CSVCL_ANY4(*s))
                {
                    sn = snatched; *sn = *s; sn++;
                    current_state = CSV_ST_NO_QUOTE_MODE;
                }
                break;

            case CSV_ST_LEADING_QUOTE:
                if (CSVCL_QUOTE(*s))
                {
                    sn = snatched;
                    current_state = CSV_ST_INTERNAL_QUOTE;
                }
                else if (CSVCL_ANY5(*s))
                {
                    sn = snatched;
                    *sn = *s; sn++;
                    current_state = CSV_ST_WITH_QUOTE_MODE;
                }
                break;

            case CSV_ST_INTERNAL_QUOTE:
                if (CSVCL_SEP(*s))
                {
                    *sn = '\0'; sn = NULL;
                    SeqAppend(*newlist, xstrdup(snatched));
                    current_state = CSV_ST_SEPARATOR;
                }
                else if (CSVCL_BLANK(*s))
                {
                    current_state = CSV_ST_SPACE_AFTER_QUOTE;
                }
                else if (CSVCL_QUOTE(*s))
                {
                    *sn = *s; sn++;
                    current_state = CSV_ST_WITH_QUOTE_MODE;
                }
                else if (CSVCL_ANY6(*s))
                {
                    snatched[0] = '\0'; sn++;
                    current_state = CSV_ST_ERROR;
                }
                break;

            case CSV_ST_WITH_QUOTE_MODE:
                if (CSVCL_QUOTE(*s))
                {
                    current_state = CSV_ST_INTERNAL_QUOTE;
                }
                else if (CSVCL_ANY7(*s))
                {
                    *sn = *s; sn++;
                    current_state = CSV_ST_WITH_QUOTE_MODE;
                }
                break;

            case CSV_ST_SPACE_AFTER_QUOTE:
                if (CSVCL_SEP(*s))
                {
                    sn = NULL;
                    SeqAppend(*newlist, xstrdup(snatched));
                    current_state = CSV_ST_SEPARATOR;
                }
                else if (CSVCL_BLANK(*s))
                {
                    if (sn != NULL)
                    {
                        *sn = '\0';
                    }
                    sn = NULL;
                    current_state = CSV_ST_SPACE_AFTER_QUOTE;
                }
                else if (CSVCL_ANY8(*s))
                {
                    snatched[0] = '\0'; sn = NULL;
                    current_state = CSV_ST_ERROR;
                }
                break;

            default:
                ret = CSV_ERR_UNKNOWN_STATE;
                goto clean;
        }
    }

    assert(*s == '\0');

    if (current_state != CSV_ST_LEADING_QUOTE &&
        current_state != CSV_ST_WITH_QUOTE_MODE )
    {
        if (sn != NULL)
        {
            *sn = *s;                            /* write the trailing '\0' */
            sn = NULL;
        }

        /* Trim trailing CRLF. */
        if(current_state == CSV_ST_NO_QUOTE_MODE ||
           current_state == CSV_ST_PRE_START_SPACE)
        {
            int len = strlen(snatched);
            if (len > 1 && snatched[len - 2] == '\r' && snatched[len - 1] == '\n')
            {
                snatched[len - 2] = '\0';
            }
        }

        SeqAppend(*newlist, xstrdup(snatched));
        snatched[0] = '\0';
    }
    else                                     /* LEADING_QUOTE or WITH_QUOTE */
    {
        ret = CSV_ERR_UNEXPECTED_END;
        goto clean;
    }

    free(snatched);

    return CSV_ERR_OK;

clean:
    if (newlist)
    {
        SeqDestroy(*newlist);
    }
    free(snatched);
    return ret;
}

Seq *SeqParseCsvString(const char *string)
{
    Seq *newlist = SeqNew(16, free);

    if (LaunchCsvAutomata(string, &newlist) != CSV_ERR_OK)
    {
        return NULL;
    }

    return newlist;
}

char *GetCsvLineNext(FILE *fp)
{
    if (!fp)
    {
        return NULL;
    }

    Writer *buffer = StringWriter();

    char prev = 0;

    for (;;)
    {
        int current = fgetc(fp);

        if (current == EOF || feof(fp))
        {
            break;
        }

        WriterWriteChar(buffer, current);

        if ((current == '\n') && (prev == '\r'))
        {
            break;
        }

        prev = current;
    }

    if (StringWriterLength(buffer) <= 0)
    {
        WriterClose(buffer);
        return NULL;
    }

    return StringWriterClose(buffer);
}
