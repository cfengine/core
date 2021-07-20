#include <platform.h>
#include <logging.h>
#include <definitions.h> // CF_BUFSIZE
#include <known_dirs.h>  // GetStateDir()
#include <file_lib.h>    // FILE_SEPARATOR
#include <alloc.h>       // xstrdup()
#include <db_structs.h>  // observable_strings
#include <string_lib.h>  // StringEqual
#include <observables.h>

/**
 * GetObservableNames guarantees to return an array of CF_OBSERVABLES size with
 * non-NULL strings if using built-in, will generate "observable[n]" name, if
 * reading ts_key file and the item is "spare" then generating "spare[n]" where
 * 'n' is the id number which could be helpful in debugging raw observables
 * data.
 *
 * This function is similar to Nova_LoadSlots() in
 * libpromises/monitoring_read.c If we refactor for dynamic observables instead
 * of hard coded and limited to 100 then we likely should change here and there
 * or refactor to have this ts_key read/parse code in one shared place.
 */
char **GetObservableNames(const char *ts_key_path)
{
    char buf[CF_BUFSIZE];
    const char *filename;
    char **temp = xmalloc(CF_OBSERVABLES * sizeof(char *));

    if (ts_key_path == NULL)
    {
        snprintf(
            buf, CF_BUFSIZE - 1, "%s%cts_key", GetStateDir(), FILE_SEPARATOR);
        filename = buf;
    }
    else
    {
        filename = ts_key_path;
    }

    FILE *f = safe_fopen(filename, "r");
    if (f == NULL)
    {
        for (int i = 0; i < CF_OBSERVABLES; ++i)
        {
            if (i < observables_max)
            {
                temp[i] = xstrdup(observable_strings[i]);
            }
            else
            {
                snprintf(buf, CF_MAXVARSIZE, "observable[%d]", i);
                temp[i] = xstrdup(buf);
            }
        }
    }
    else
    {
        for (int i = 0; i < CF_OBSERVABLES; ++i)
        {
            char line[CF_MAXVARSIZE];

            char name[CF_MAXVARSIZE], desc[CF_MAXVARSIZE];
            char units[CF_MAXVARSIZE] = "unknown";
            double expected_min = 0.0;
            double expected_max = 100.0;
            int consolidable = true;

            if (fgets(line, CF_MAXVARSIZE, f) == NULL)
            {
                Log(LOG_LEVEL_ERR,
                    "Error trying to read ts_key from file '%s'. (fgets: %s)",
                    filename,
                    GetErrorStr());
                break;
            }

            const int fields = sscanf(
                line,
                "%*d,%1023[^,],%1023[^,],%1023[^,],%lf,%lf,%d",
                name,
                desc,
                units,
                &expected_min,
                &expected_max,
                &consolidable);

            if ((fields != 2) && (fields != 6))
            {
                Log(LOG_LEVEL_ERR, "Wrong line format in ts_key: %s", line);
            }

            if (StringEqual(name, "spare"))
            {
                temp[i] = xstrdup(name);
            }
            else
            {
                snprintf(buf, CF_MAXVARSIZE, "spare[%d]", i);
                temp[i] = xstrdup(buf);
            }
        }
        fclose(f);
    }
    return temp;
}
