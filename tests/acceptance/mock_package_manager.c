#include <cf3.defs.h>

#include <rlist.h>
#include <generic_agent.h> // Syntax()
#include <cf-windows-functions.h>

static char AVAILABLE_PACKAGES_FILE_NAME[PATH_MAX];
static char INSTALLED_PACKAGES_FILE_NAME[PATH_MAX];

static const int MAX_PACKAGE_ENTRY_LENGTH = 256;

#define DEFAULT_ARCHITECTURE "x666"

#define Error(msg) fprintf(stderr, "%s:%d: %s", __FILE__, __LINE__, msg)

static const struct option OPTIONS[] =
{
    {"clear-installed", no_argument, 0, 'c'},
    {"clear-available", no_argument, 0, 'C'},
    {"list-installed", no_argument, 0, 'l'},
    {"list-available", no_argument, 0, 'L'},
    {"populate-available", required_argument, 0, 'P'},
    {"add", required_argument, 0, 'a'},
    {"delete", required_argument, 0, 'd'},
    {"reinstall", required_argument, 0, 'r'},
    {"update", required_argument, 0, 'u'},
    {"addupdate", required_argument, 0, 'U'},
    {"verify", required_argument, 0, 'v'},
    {NULL, 0, 0, '\0'}
};

static const char *HINTS[] =
{
    "Clear all installed imaginary packages",
    "Clear all available imaginary packages",
    "List installed imarginary packages",
    "List imaginary packages available to be installed",
    "Add an imaginary package to list of available",
    "Add an imaginary package",
    "Delete a previously imagined package",
    "Reinstall an imaginary package",
    "Update a previously imagined package",
    "Add or update an imaginary package",
    "Verify a previously imagined package",
    NULL
};

typedef struct
{
    char *name;
    char *version;
    char *arch;
} Package;

typedef struct
{
    char *name;
    char *version;
    char *arch;
} PackagePattern;

/******************************************************************************/

static char *SerializePackage(Package *package)
{
    char *s;

    xasprintf(&s, "%s:%s:%s", package->name, package->version, package->arch);
    return s;
}

/******************************************************************************/

static char *SerializePackagePattern(PackagePattern *pattern)
{
    char *s;

    xasprintf(&s, "%s:%s:%s", pattern->name ? pattern->name : "*",
              pattern->version ? pattern->version : "*", pattern->arch ? pattern->arch : "*");
    return s;
}

/******************************************************************************/

static Package *DeserializePackage(const char *entry)
{
    Package *package = xcalloc(1, sizeof(Package));

    char *entry_copy = xstrdup(entry);

    package->name = strtok(entry_copy, ":");
    package->version = strtok(NULL, ":");
    package->arch = strtok(NULL, ":");

    if (package->name == NULL || strcmp(package->name, "*") == 0
        || package->version == NULL || strcmp(package->version, "*") == 0
        || package->arch == NULL || strcmp(package->arch, "*") == 0)
    {
        fprintf(stderr, "Incomplete package specification: %s:%s:%s\n", package->name, package->version, package->arch);
        exit(255);
    }

    return package;
}

/******************************************************************************/

static bool IsWildcard(const char *str)
{
    return str && (strcmp(str, "*") == 0 || strcmp(str, "") == 0);
}

/******************************************************************************/

static void CheckWellformedness(const char *str)
{
    if (str && strchr(str, '*') != NULL)
    {
        fprintf(stderr, "* is encountered in pattern not as wildcard: '%s'\n", str);
        exit(255);
    }
}

/******************************************************************************/

static PackagePattern *NewPackagePattern(const char *name, const char *version, const char *arch)
{
    PackagePattern *pattern = xcalloc(1, sizeof(PackagePattern));

    pattern->name = name ? xstrdup(name) : NULL;
    pattern->version = version ? xstrdup(version) : NULL;
    pattern->arch = arch ? xstrdup(arch) : NULL;
    return pattern;
}

/******************************************************************************/

static PackagePattern *DeserializePackagePattern(const char *entry)
{
//PackagePattern *pattern = xcalloc(1, sizeof(PackagePattern));

    char *entry_copy = xstrdup(entry);

    char *name = strtok(entry_copy, ":");

    if (IsWildcard(name))
    {
        name = NULL;
    }
    CheckWellformedness(name);

    char *version = strtok(NULL, ":");

    if (IsWildcard(version))
    {
        version = NULL;
    }
    CheckWellformedness(version);

    char *arch = strtok(NULL, ":");

    if (arch == NULL)
    {
        arch = DEFAULT_ARCHITECTURE;
    }
    else if (IsWildcard(arch))
    {
        arch = NULL;
    }
    CheckWellformedness(arch);

    if (strtok(NULL, ":") != NULL)
    {
        fprintf(stderr, "Too many delimiters are encountered in pattern: %s\n", entry);
        exit(255);
    }

    return NewPackagePattern(name, version, arch);
}

/******************************************************************************/

static Seq *ReadPackageEntries(const char *database_filename)
{
    FILE *packages_file = fopen(database_filename, "r");
    Seq *packages = SeqNew(1000, NULL);

    if (packages_file != NULL)
    {
        char serialized_package[MAX_PACKAGE_ENTRY_LENGTH];

        while (fscanf(packages_file, "%s\n", serialized_package) != EOF)
        {
            Package *package = DeserializePackage(serialized_package);

            SeqAppend(packages, package);
        }

        fclose(packages_file);
    }

    return packages;
}

/******************************************************************************/

static void SavePackages(const char *database_filename, Seq *package_entries)
{
    FILE *packages_file = fopen(database_filename, "w");

    for (size_t i = 0; i < SeqLength(package_entries); i++)
    {
        fprintf(packages_file, "%s\n", SerializePackage(SeqAt(package_entries, i)));
    }

    fclose(packages_file);
}

/******************************************************************************/

static PackagePattern *MatchAllVersions(const Package *p)
{
    return NewPackagePattern(p->name, NULL, p->arch);
}

/******************************************************************************/

static PackagePattern *MatchSame(const Package *p)
{
    return NewPackagePattern(p->name, p->version, p->arch);
}

/******************************************************************************/

static bool MatchPackage(PackagePattern *a, Package *b)
{
    return (a->name == NULL || strcmp(a->name, b->name) == 0) &&
        (a->version == NULL || strcmp(a->version, b->version) == 0) &&
        (a->arch == NULL || strcmp(a->arch, b->arch) == 0);
}

/******************************************************************************/

static Seq *FindPackages(const char *database_filename, PackagePattern *pattern)
{
    Seq *db = ReadPackageEntries(database_filename);
    Seq *matching = SeqNew(1000, NULL);

    for (size_t i = 0; i < SeqLength(db); i++)
    {
        Package *package = SeqAt(db, i);

        if (MatchPackage(pattern, package))
        {
            SeqAppend(matching, package);
        }
    }

    return matching;
}

/******************************************************************************/

static void ShowPackages(FILE *out, Seq *package_entries)
{
    for (size_t i = 0; i < SeqLength(package_entries); i++)
    {
        fprintf(out, "%s\n", SerializePackage(SeqAt(package_entries, i)));
    }
}

/******************************************************************************/

static void ClearPackageList(const char *db_file_name)
{
    FILE *packages_file = fopen(db_file_name, "w");

    if (packages_file == NULL)
    {
        fprintf(stderr, "fopen(%s): %s", INSTALLED_PACKAGES_FILE_NAME, strerror(errno));
        exit(255);
    }
    fclose(packages_file);
}

/******************************************************************************/

static void ClearInstalledPackages(void)
{
    ClearPackageList(INSTALLED_PACKAGES_FILE_NAME);
}

/******************************************************************************/

static void ClearAvailablePackages(void)
{
    ClearPackageList(AVAILABLE_PACKAGES_FILE_NAME);
}

/******************************************************************************/

static void AddPackage(PackagePattern *pattern)
{
    fprintf(stderr, "Trying to install all packages matching pattern %s\n", SerializePackagePattern(pattern));

    Seq *matching_available = FindPackages(AVAILABLE_PACKAGES_FILE_NAME, pattern);

    if (SeqLength(matching_available) == 0)
    {
        fprintf(stderr, "Unable to find any package matching %s\n", SerializePackagePattern(pattern));
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < SeqLength(matching_available); i++)
    {
        Package *p = SeqAt(matching_available, i);

        PackagePattern *pat = MatchAllVersions(p);

        if (SeqLength(FindPackages(INSTALLED_PACKAGES_FILE_NAME, pat)) > 0)
        {
            fprintf(stderr, "Package %s is already installed.\n", SerializePackage(p));
            exit(EXIT_FAILURE);
        }
    }

    Seq *installed_packages = ReadPackageEntries(INSTALLED_PACKAGES_FILE_NAME);

    for (size_t i = 0; i < SeqLength(matching_available); i++)
    {
        Package *p = SeqAt(matching_available, i);

        SeqAppend(installed_packages, p);
        fprintf(stderr, "Successfully installed package %s\n", SerializePackage(p));
    }

    SavePackages(INSTALLED_PACKAGES_FILE_NAME, installed_packages);
    exit(EXIT_SUCCESS);
}

/******************************************************************************/

static void PopulateAvailable(const char *arg)
{
    Package *p = DeserializePackage(arg);
    PackagePattern *pattern = MatchSame(p);

    if (SeqLength(FindPackages(AVAILABLE_PACKAGES_FILE_NAME, pattern)) > 0)
    {
        fprintf(stderr, "Skipping already available package %s\n", SerializePackage(p));
        return;
    }

    Seq *available_packages = ReadPackageEntries(AVAILABLE_PACKAGES_FILE_NAME);

    SeqAppend(available_packages, p);
    SavePackages(AVAILABLE_PACKAGES_FILE_NAME, available_packages);
}

/******************************************************************************/

int main(int argc, char *argv[])
{
    extern char *optarg;
    int option_index = 0;
    int c;

#ifdef __MINGW32__
    InitializeWindows();
#endif

    char *workdir = getenv("CFENGINE_TEST_OVERRIDE_WORKDIR");
    char *tempdir = getenv("TEMP");

    if (!workdir && !tempdir)
    {
        fprintf(stderr, "Please set either CFENGINE_TEST_OVERRIDE_WORKDIR or TEMP environment variables\n"
                "to a valid directory.\n");
        return 2;
    }

    xsnprintf(AVAILABLE_PACKAGES_FILE_NAME, 256,
             "%s/cfengine-mock-package-manager-available", workdir ? workdir : tempdir);
    xsnprintf(INSTALLED_PACKAGES_FILE_NAME, 256,
             "%s/cfengine-mock-package-manager-installed", workdir ? workdir : tempdir);

    while ((c = getopt_long(argc, argv, "", OPTIONS, &option_index)) != EOF)
    {
        PackagePattern *pattern = NULL;

        switch (c)
        {
        case 'c':
            ClearInstalledPackages();
            break;

        case 'C':
            ClearAvailablePackages();
            break;

        case 'l':
            {
                Seq *installed_packages = ReadPackageEntries(INSTALLED_PACKAGES_FILE_NAME);
                ShowPackages(stdout, installed_packages);
            }
            break;

        case 'L':
            {
                Seq *available_packages = ReadPackageEntries(AVAILABLE_PACKAGES_FILE_NAME);
                ShowPackages(stdout, available_packages);
            }
            break;

        case 'a':
            pattern = DeserializePackagePattern(optarg);
            AddPackage(pattern);
            break;

        case 'P':
            PopulateAvailable(optarg);
            break;

            /* case 'd': */
            /*         DeletePackage(pattern); */
            /*         break; */

            /* case 'r': */
            /*    ReinstallPackage(pattern); */
            /*    break; */

            /* case 'u': */
            /*    UpdatePackage(pattern); */
            /*    break; */

            /* case 'U': */
            /*    AddUpdatePackage(pattern); */
            /*    break; */

            /* case 'v': */
            /*         VerifyPackage(pattern); */
            /*         break; */

        default:
            {
                Writer *w = FileWriter(stdout);
                WriterWriteHelp(w, "mock-package-manager - pretend that you are managing packages!", OPTIONS, HINTS, false);
                FileWriterDetach(w);
            }
            exit(EXIT_FAILURE);
        }

    }

    return 0;
}
