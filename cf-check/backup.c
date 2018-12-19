#include <platform.h>
#include <stdio.h>
#include <utilities.h>
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
