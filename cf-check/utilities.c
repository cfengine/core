#include <platform.h>
#include <utilities.h>
#include <dir.h>
#include <sequence.h>
#include <string_lib.h>
#include <alloc.h>
#include <logging.h>

bool copy_file(const char *src, const char *dst)
{
    assert(src != NULL);
    assert(dst != NULL);

    Log(LOG_LEVEL_INFO, "Copying: '%s' -> '%s'", src, dst);

    FILE *in = fopen(src, "r");
    if (in == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not open '%s' (%s)", src, strerror(errno));
        return false;
    }

    FILE *out = fopen(dst, "w");
    if (out == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not open '%s' (%s)", dst, strerror(errno));
        fclose(in);
        return false;
    }

    size_t bytes_in = 0;
    size_t bytes_out = 0;
    bool ret = true;
    do
    {
#define BUFSIZE 1024
        char buf[BUFSIZE] = {0};

        bytes_in = fread(buf, sizeof(char), sizeof(buf), in);
        bytes_out = fwrite(buf, sizeof(char), bytes_in, out);
        while (bytes_out < bytes_in && !ferror(out))
        {
            bytes_out += fwrite(
                buf + bytes_out, sizeof(char), bytes_in - bytes_out, out);
        }
    } while (!feof(in) && !ferror(in) && !ferror(out) &&
             bytes_in == bytes_out);

    if (ferror(in))
    {
        Log(LOG_LEVEL_ERR, "Error encountered while reading '%s'", src);
        ret = false;
    }
    else if (ferror(out))
    {
        Log(LOG_LEVEL_ERR, "Error encountered while writing '%s'", dst);
        ret = false;
    }
    else if (bytes_in != bytes_out)
    {
        Log(LOG_LEVEL_ERR, "Did not copy the whole file");
        ret = false;
    }

    const int i = fclose(in);
    if (i != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Error encountered while closing '%s' (%s)",
            src,
            strerror(errno));
        ret = false;
    }
    const int o = fclose(out);
    if (o != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Error encountered while closing '%s' (%s)",
            dst,
            strerror(errno));
        ret = false;
    }
    return ret;
}

const char *filename_part(const char *path)
{
    assert(path != NULL);

    const char *filename = strrchr(path, '/');

    if (filename != NULL)
    {
        filename += 1;
    }
    else
    {
        filename = path;
    }

    if (filename[0] == '\0')
    {
        return NULL;
    }

    return filename;
}

bool copy_file_to_folder(const char *src, const char *dst_dir)
{
    assert(src != NULL);
    assert(dst_dir != NULL);

    const char *filename = filename_part(src);
    if (filename == NULL)
    {
        Log(LOG_LEVEL_ERR, "Cannot find filename in '%s'", src);
        return false;
    }

    char dst[PATH_MAX] = {0};
    const int s = snprintf(dst, PATH_MAX, "%s%s", dst_dir, filename);
    if (s >= PATH_MAX)
    {
        Log(LOG_LEVEL_ERR, "Copy destination path too long: '%s...'", dst);
        return false;
    }

    if (!copy_file(src, dst))
    {
        Log(LOG_LEVEL_ERR, "Copying '%s' failed", filename);
        return false;
    }

    return true;
}

Seq *argv_to_seq(int argc, char **argv)
{
    assert(argc > 0);
    assert(argv != NULL);
    assert(argv[0] != NULL);

    Seq *args = SeqNew(argc, NULL);
    for (int i = 0; i < argc; ++i)
    {
        SeqAppend(args, argv[i]);
    }
    return args;
}

char *join_paths_alloc(const char *dir, const char *leaf)
{
    if (StringEndsWith(dir, "/"))
    {
        return StringConcatenate(2, dir, leaf);
    }
    return StringConcatenate(3, dir, "/", leaf);
}

Seq *ls(const char *dir, const char *extension)
{
    Dir *dirh = DirOpen(dir);
    if (dirh == NULL)
    {
        return NULL;
    }

    Seq *contents = SeqNew(10, free);

    const struct dirent *dirp;

    while ((dirp = DirRead(dirh)) != NULL)
    {
        const char *name = dirp->d_name;
        if (extension == NULL || StringEndsWithCase(name, extension, true))
        {
            SeqAppend(contents, join_paths_alloc(dir, name));
        }
    }
    DirClose(dirh);

    return contents;
}

Seq *default_lmdb_files()
{
    const char *state = "/var/cfengine/state/";
    Log(LOG_LEVEL_INFO,
        "No filenames specified, defaulting to .lmdb files in %s",
        state);
    Seq *files = ls(state, ".lmdb");
    if (files == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not open %s", state);
    }
    return files;
}

Seq *argv_to_lmdb_files(int argc, char **argv)
{
    if (argc <= 1)
    {
        return default_lmdb_files();
    }

    return argv_to_seq(argc - 1, argv + 1);
}
