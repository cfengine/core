/*

 * The original pcrs code came from ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/Contrib/pcrs-0.0.3-src.tar.gz

 * This is a rewrite of PCRS with the same API and behavior.

 */



#include <platform.h>

#include <pcre_wrap.h>

static int pcre_wrap_parse_perl_options(const char *optstring, int *flags);

static pcre_wrap_substitute *pcre_wrap_compile_replacement(const char *replacement, int trivialflag,
                                                           int capturecount, int *errptr);

// Retain the same error messages as PCRS
const char *pcre_wrap_strerror(const int error)
{
    if (error < 0)
    {
        switch (error)
        {
            /* Passed-through PCRE error: */
        case PCRE_ERROR_NOMEMORY:     return "(pcre:) No memory";

            /* Shouldn't happen unless PCRE or PCRE_WRAP bug, or user messed with compiled job: */
        case PCRE_ERROR_NULL:         return "(pcre:) NULL code or subject or ovector";
        case PCRE_ERROR_BADOPTION:    return "(pcre:) Unrecognized option bit";
        case PCRE_ERROR_BADMAGIC:     return "(pcre:) Bad magic number in code";
        case PCRE_ERROR_UNKNOWN_NODE: return "(pcre:) Bad node in pattern";

            /* Can't happen / not passed: */
        case PCRE_ERROR_NOSUBSTRING:  return "(pcre:) Fire in power supply"; 
        case PCRE_ERROR_NOMATCH:      return "(pcre:) Water in power supply";

            /* PCRE_WRAP errors: */
        case PCRE_WRAP_ERR_NOMEM:          return "(pcre_wrap:) No memory";
        case PCRE_WRAP_ERR_CMDSYNTAX:      return "(pcre_wrap:) Syntax error while parsing command";
        case PCRE_WRAP_ERR_STUDY:          return "(pcre_wrap:) PCRE error while studying the pattern";
        case PCRE_WRAP_ERR_BADJOB:         return "(pcre_wrap:) Bad job - NULL job, pattern or substitute";
        case PCRE_WRAP_WARN_BADREF:        return "(pcre_wrap:) Backreference out of range";

            /* What's that? */
        default:  return "Unknown error";
        }
    }
    /* error >= 0: No error */
    return "(pcre_wrap:) Everything's just fine. Thanks for asking.";

}

static int pcre_wrap_parse_perl_options(const char *options, int *flags)
{
    int rc = 0;
    *flags = 0;

    if (options == NULL)
    {
        return 0;
    }

    size_t max = strlen(options);
    for (size_t i = 0; i < max; i++)
    {
        switch(options[i])
        {
        case 'e': break; /* Nein! Jamais! Nunca! Well, maybe if you ask nicely... */
        case 'g': *flags |= PCRE_WRAP_GLOBAL; break;
        case 'i': rc |= PCRE_CASELESS; break;
        case 'm': rc |= PCRE_MULTILINE; break;
        case 'o': break;
        case 's': rc |= PCRE_DOTALL; break;
        case 'x': rc |= PCRE_EXTENDED; break;
        case 'U': rc |= PCRE_UNGREEDY; break;
        case 'T': *flags |= PCRE_WRAP_TRIVIAL; break;
        default: break;
        }
    }
    return rc;

}

static pcre_wrap_substitute *pcre_wrap_compile_replacement(const char *replacement, int trivialflag, int capturecount, int *errptr)
{
    int refnum = 0;
    char *text;
    pcre_wrap_substitute *rstruct;

    /*
     * Sanity check
     */
    if (replacement == NULL)
    {
        replacement = "";
    }

    size_t length = strlen(replacement);

    rstruct = calloc(1, sizeof(pcre_wrap_substitute));
    if (rstruct == NULL)
    {
        *errptr = PCRE_WRAP_ERR_NOMEM;
        return NULL;
    }

    text = calloc(1, length + 1);
    if (text == NULL)
    {
        free(rstruct);
        *errptr = PCRE_WRAP_ERR_NOMEM;
        return NULL;
    }

    size_t textpos = 0;

    if (trivialflag)
    {
        text = memcpy(text, replacement, length + 1);
        textpos = length;
    }
    else // non-trivial search and replace
    {
        int quoted = 0;
        size_t sourcepos = 0;

        while (sourcepos < length)
        {
            /* Quoting */
            if (replacement[sourcepos] == '\\' &&
                (!replacement[sourcepos+1] ||
                 // especially exclude \1 through \9 backreferences
                 (replacement[sourcepos+1] && !strchr("123456789", replacement[sourcepos+1]))))
            {
                if (quoted)
                {
                    text[textpos++] = replacement[sourcepos++];
                    quoted = 0;
                }
                else
                {
                    if (replacement[sourcepos+1] && strchr("tnrfae0", replacement[sourcepos+1]))
                    {
                        switch (replacement[++sourcepos])
                        {
                        case 't':
                            text[textpos++] = '\t';
                            break;
                        case 'n':
                            text[textpos++] = '\n';
                            break;
                        case 'r':
                            text[textpos++] = '\r';
                            break;
                        case 'f':
                            text[textpos++] = '\f';
                            break;
                        case 'a':
                            text[textpos++] = 7;
                            break;
                        case 'e':
                            text[textpos++] = 27;
                            break;
                        case '0':
                            text[textpos++] = '\0';
                            break;
                        }
                        sourcepos++;
                    }
                    else
                    {
                        quoted = 1;
                        sourcepos++;
                    }
                }
                continue;
            }

            /* Backreferences */
            bool have_backslash = (replacement[sourcepos] == '\\');
            bool have_dollar = (replacement[sourcepos] == '$');
            if ( (have_backslash || have_dollar ) &&
                 !quoted && sourcepos < length - 1)
            {
                char *symbol = NULL;
                char symbols[] = "'`+&";
                rstruct->block_length[refnum] = textpos - rstruct->block_offset[refnum];

                if (isdigit((int)replacement[sourcepos + 1])) // numeric backref
                {
                    while (sourcepos < length && isdigit((int)replacement[++sourcepos]))
                    {
                        rstruct->backref[refnum] = rstruct->backref[refnum] * 10 + replacement[sourcepos] - 48;
                    }
                    if (rstruct->backref[refnum] > capturecount)
                    {
                        *errptr = PCRE_WRAP_WARN_BADREF;
                    }
                }
                else if (have_dollar &&
                         (symbol = strchr(symbols, replacement[sourcepos + 1])) != NULL)
                {
                    if (*symbol == '+') /* $+ */
                    {
                        rstruct->backref[refnum] = capturecount;
                    }
                    else if (*symbol == '&') /* $& */
                    {
                        rstruct->backref[refnum] = 0;
                    }
                    else /* $' or $` */
                    {
                        rstruct->backref[refnum] = PCRE_WRAP_MAX_SUBMATCHES + 1 - (symbol - symbols);
                    }
                    sourcepos += 2;
                }
                else // not a backref after all
                {
                    goto plainchar;
                }

                if (rstruct->backref[refnum] < PCRE_WRAP_MAX_SUBMATCHES + 2)
                {
                    rstruct->backref_count[rstruct->backref[refnum]]++;
                    rstruct->block_offset[++refnum] = textpos;
                }
                else
                {
                    *errptr = PCRE_WRAP_WARN_BADREF;
                }
                continue;
            }

          plainchar:
            /* Plain chars are copied */
            text[textpos++] = replacement[sourcepos++];
            quoted = 0;
        }
    }

    rstruct->text = text;
    rstruct->backrefs = refnum;
    rstruct->block_length[refnum] = textpos - rstruct->block_offset[refnum];

    return rstruct;
}

pcre_wrap_job *pcre_wrap_free_job(pcre_wrap_job *job)
{
    if (job == NULL)
    {
        return NULL;
    }

    // grab the next node and destroy the current head
    pcre_wrap_job *next = job->next;
    if (job->pattern != NULL)
    {
        free(job->pattern);
    }

    if (job->hints != NULL)
    {
        free(job->hints);
    }

    if (job->substitute != NULL)
    {
        if (job->substitute->text != NULL)
        {
            free(job->substitute->text);
        }
        free(job->substitute);
    }
    free(job);

    return next;
}

pcre_wrap_job *pcre_wrap_compile(const char *pattern, const char *substitute, const char *options, int *errptr)
{
    int flags;
    int capturecount;
    const char *error;

    *errptr = 0;

    if (pattern == NULL)
    {
        pattern = "";
    }

    if (substitute == NULL)
    {
        substitute = "";
    }

    pcre_wrap_job *newjob = calloc(1, sizeof(pcre_wrap_job));
    if (newjob == NULL)
    {
        *errptr = PCRE_WRAP_ERR_NOMEM;
        return NULL;
    }

    newjob->options = pcre_wrap_parse_perl_options(options, &flags);
    newjob->flags = flags;

    newjob->pattern = pcre_compile(pattern, newjob->options, &error, errptr, NULL);
    if (newjob->pattern == NULL)
    {
        pcre_wrap_free_job(newjob);
        return NULL;
    }

    newjob->hints = pcre_study(newjob->pattern, 0, &error);
    if (error != NULL)
    {
        *errptr = PCRE_WRAP_ERR_STUDY;
        pcre_wrap_free_job(newjob);
        return NULL;
    }

    *errptr = pcre_fullinfo(newjob->pattern, newjob->hints, PCRE_INFO_CAPTURECOUNT, &capturecount);
    if (*errptr < 0)
    {
        pcre_wrap_free_job(newjob);
        return NULL;
    }

    newjob->substitute = pcre_wrap_compile_replacement(substitute, newjob->flags & PCRE_WRAP_TRIVIAL, capturecount, errptr);
    if (newjob->substitute == NULL)
    {
        pcre_wrap_free_job(newjob);
        return NULL;
    }

    return newjob;
}

int pcre_wrap_execute(pcre_wrap_job *job, char *subject, size_t subject_length, char **result, size_t *result_length)
{
    int offsets[3 * PCRE_WRAP_MAX_SUBMATCHES],
        max_matches = PCRE_WRAP_MAX_MATCH_INIT;
    int offset = 0;
    int matches_found = 0;
    size_t newsize;
    char *result_offset;
    int submatches = 0;

    if (job == NULL || job->pattern == NULL || job->substitute == NULL)
    {
        *result = NULL;
        return(PCRE_WRAP_ERR_BADJOB);
    }

    pcre_wrap_match *matches = calloc(max_matches, sizeof(pcre_wrap_match));
    if (matches == NULL)
    {
        *result = NULL;
        return(PCRE_WRAP_ERR_NOMEM);
    }

    // starting size, to be incremented hereafter
    newsize = subject_length;

    int current_match = 0;
    while (TRUE)
    {
        submatches = pcre_exec(job->pattern, job->hints, subject, (int)subject_length, offset, 0, offsets, 3 * PCRE_WRAP_MAX_SUBMATCHES);

        if (submatches <= 0)
        {
            break;
        }

        job->flags |= PCRE_WRAP_SUCCESS;
        matches[current_match].submatches = submatches;

        for (int submatch = 0; submatch < submatches; submatch++)
        {
            matches[current_match].submatch_offset[submatch] = offsets[2 * submatch];

            /* compatibility note: Missing optional submatches have length -1-(-1)==0 */
            matches[current_match].submatch_length[submatch] = offsets[2 * submatch + 1] - offsets[2 * submatch]; 

            /* reserve mem for *each* submatch reference! */
            newsize += matches[current_match].submatch_length[submatch] * job->substitute->backref_count[submatch];
        }
        /* plus replacement text size minus match text size */
        newsize += strlen(job->substitute->text) - matches[current_match].submatch_length[0]; 

        /* plus the chunk before match */
        matches[current_match].submatch_offset[PCRE_WRAP_MAX_SUBMATCHES] = 0;
        matches[current_match].submatch_length[PCRE_WRAP_MAX_SUBMATCHES] = offsets[0];
        newsize += ((size_t) offsets[0]) * job->substitute->backref_count[PCRE_WRAP_MAX_SUBMATCHES];

        /* plus the chunk after match */
        matches[current_match].submatch_offset[PCRE_WRAP_MAX_SUBMATCHES + 1] = offsets[1];
        matches[current_match].submatch_length[PCRE_WRAP_MAX_SUBMATCHES + 1] = subject_length - offsets[1] - 1;
        newsize += (subject_length - offsets[1]) * job->substitute->backref_count[PCRE_WRAP_MAX_SUBMATCHES + 1];

        current_match++;
        if (current_match >= max_matches) // extend the storage if needed
        {
            max_matches = (int)(max_matches * PCRE_WRAP_MAX_MATCH_GROW);

            pcre_wrap_match *temp = realloc(matches, max_matches * sizeof(pcre_wrap_match));
            if (temp == NULL)
            {
                free(matches);
                *result = NULL;
                return(PCRE_WRAP_ERR_NOMEM);
            }
            matches = temp;
        }

        // break if the search was not global
        if (!(job->flags & PCRE_WRAP_GLOBAL))
        {
            break;
        }

        // skip empty matches
        if (offsets[1] == offset)
        {
            if ((size_t)offset < subject_length)
            {
                offset++;
            }
            else
            {
                break;
            }
        }
        else // go to next match
        {
            offset = offsets[1];
        }
    }

    if (submatches < PCRE_ERROR_NOMATCH)
    {
        free(matches);
        return submatches;
    }
    matches_found = current_match;

    *result = malloc(newsize + 1); // caller will free this
    if (*result == NULL)
    {
        free(matches);
        return PCRE_WRAP_ERR_NOMEM;
    }
    else
    {
        (*result)[newsize] = '\0';
    }

    offset = 0;
    result_offset = *result;

    for (int matchpos = 0; matchpos < matches_found; matchpos++)
    {
        // start with the text before the match
        memcpy(result_offset, subject + offset, (size_t)matches[matchpos].submatch_offset[0] - offset); 
        result_offset += matches[matchpos].submatch_offset[0] - offset;

        // copy every segment
        for (int segment = 0; segment <= job->substitute->backrefs; segment++)
        {
            // copy the segment's text
            memcpy(result_offset, job->substitute->text + job->substitute->block_offset[segment], job->substitute->block_length[segment]);
            result_offset += job->substitute->block_length[segment];

            if (segment != job->substitute->backrefs
                && job->substitute->backref[segment] < PCRE_WRAP_MAX_SUBMATCHES + 2
                && job->substitute->backref[segment] < matches[matchpos].submatches
                && matches[matchpos].submatch_length[job->substitute->backref[segment]] > 0)
            {
                // copy the referenced submatch
                memcpy(
                    result_offset,
                    subject + matches[matchpos].submatch_offset[job->substitute->backref[segment]],
                    matches[matchpos].submatch_length[job->substitute->backref[segment]]
                    );
                result_offset += matches[matchpos].submatch_length[job->substitute->backref[segment]];
            }
        }
        offset =  matches[matchpos].submatch_offset[0] + matches[matchpos].submatch_length[0];
    }

    // copy the rest and finish
    memcpy(result_offset, subject + offset, subject_length - offset);

    *result_length = newsize;
    free(matches);
    return matches_found;

}
