#include <platform.h>
#include <backup.h>

#if defined (__MINGW32__)

int backup_main(int argc, char **argv)
{
    printf("backup not supported on Windows\n");
    return 1;
}

#elif ! defined (LMDB)

int backup_main(int argc, char **argv)
{
    printf("backup only implemented for LMDB.\n");
    return 1;
}

#else

#include <stdio.h>

bool copy_file(const char *src, const char *dst)
{
    assert(src != NULL);
    assert(dst != NULL);

    printf("Copying: '%s' -> '%s'\n", src, dst);

    FILE *in = fopen(src, "r");
    if (in == NULL)
    {
        printf("Could not open '%s' (%s)\n", src, strerror(errno));
        return false;
    }

    FILE *out = fopen(dst, "w");
    if (out == NULL)
    {
        printf("Could not open '%s' (%s)\n", dst, strerror(errno));
        fclose(in);
        return false;
    }

    size_t bytes_in = 0;
    size_t bytes_out = 0;
    bool ret = true;
    do
    {
        #define BUFSIZE 1024
        char buf[BUFSIZE] = { 0 };

        bytes_in = fread(buf, sizeof(char), sizeof(buf), in);
        bytes_out = fwrite(buf, sizeof(char), bytes_in, out);
        while (bytes_out < bytes_in && !ferror(out))
        {
            bytes_out += fwrite(buf + bytes_out, sizeof(char), bytes_in - bytes_out, out);
        }
    } while (!feof(in) && !ferror(in) && !ferror(out) && bytes_in == bytes_out);

    if (ferror(in))
    {
        printf("Error encountered while reading '%s'\n", src);
        ret = false;
    }
    else if (ferror(out))
    {
        printf("Error encountered while writing '%s'\n", dst);
        ret = false;
    }
    else if (bytes_in != bytes_out)
    {
        printf("Did not copy the whole file\n");
        ret = false;
    }

    const int i = fclose(in);
    if (i != 0)
    {
        printf("Error encountered while closing '%s' (%s)\n", src, strerror(errno));
        ret = false;
    }
    const int o = fclose(out);
    if (o != 0)
    {
        printf("Error encountered while closing '%s' (%s)\n", dst, strerror(errno));
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
        printf("Cannot find filename in '%s'\n", src);
        return false;
    }

    char dst[PATH_MAX] = { 0 };
    const int s = snprintf(dst, PATH_MAX, "%s%s", dst_dir, filename);
    if (s >= PATH_MAX)
    {
        printf("Copy destination path too long: '%s...'\n", dst);
        return false;
    }

    if (!copy_file(src, dst))
    {
        printf("Copying '%s' failed\n", filename);
        return false;
    }

    return true;
}

const char *create_backup_dir()
{
    static char backup_dir[PATH_MAX];
    const char *const backup_root = "/var/cfengine/backup/";

    if (mkdir(backup_root, 0700) != 0)
    {
        if (errno != EEXIST)
        {
            printf("Could not create directory '%s' (%s)\n", backup_root, strerror(errno));
            return NULL;
        }
    }

    time_t ts = time(NULL);
    if (ts == (time_t) -1)
    {
        printf("Could not get current time\n");
        return NULL;
    }

    const int n = snprintf(backup_dir, PATH_MAX, "%s%jd/", backup_root, (intmax_t) ts);
    if (n >= PATH_MAX)
    {
        printf("Backup path too long: %jd/%jd\n", (intmax_t) n, (intmax_t) PATH_MAX);
        return NULL;
    }

    if (mkdir(backup_dir, 0700) != 0)
    {
        printf("Could not create directory '%s' (%s)\n", backup_dir, strerror(errno));
        return NULL;
    }

    return backup_dir;
}

int backup_files(int count, char **files)
{
    if (count <= 0)
    {
        printf("Need to supply filename(s)\n");
        return 1;
    }

    const char *backup_dir = create_backup_dir();

    printf("Backing up to '%s'\n", backup_dir);

    for (int i = 0; i < count; ++i)
    {
        if (!copy_file_to_folder(files[i], backup_dir))
        {
            printf("Copying '%s' failed\n", files[i]);
            return 1;
        }
    }
    return 0;
}

int backup_main(int count, char **argv)
{
    return backup_files(count - 1, argv + 1);
}

#endif
