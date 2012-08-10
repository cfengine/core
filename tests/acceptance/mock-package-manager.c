#include "cf3.defs.h"

static char AVAILABLE_PACKAGES_FILE_NAME[PATH_MAX];
static char INSTALLED_PACKAGES_FILE_NAME[PATH_MAX];

static const int MAX_PACKAGE_ENTRY_LENGTH = 256;

#define DEFAULT_ARCHITECTURE "x666"

#define Error(msg) fprintf(stderr, "%s:%d: %s", __FILE__, __LINE__, msg)

static const char *ID = "The CFEngine mock package manager tricks cf-agent into thinking\n"
    "it has actually installed something on the system. Knock yourself out!";

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

static Rlist *ReadPackageEntries(const char *database_filename)
{
    FILE *packages_file = fopen(database_filename, "r");
    Rlist *packages = NULL;

    if (packages_file != NULL)
    {
        char serialized_package[MAX_PACKAGE_ENTRY_LENGTH];

        while (fscanf(packages_file, "%s\n", serialized_package) != EOF)
        {
            Package *package = DeserializePackage(serialized_package);

            AppendRlistAlien(&packages, package);
        }

        fclose(packages_file);
    }

    return packages;
}

/******************************************************************************/

static void SavePackages(const char *database_filename, Rlist *package_entries)
{
    Rlist *rp = NULL;
    FILE *packages_file = fopen(database_filename, "w");

    for (rp = package_entries; rp != NULL; rp = rp->next)
    {
        fprintf(packages_file, "%s\n", SerializePackage((Package *) rp->item));
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

static Rlist *FindPackages(const char *database_filename, PackagePattern *pattern)
{
    Rlist *db = ReadPackageEntries(database_filename);
    Rlist *matching = NULL;

    Rlist *rp = NULL;

    for (rp = db; rp != NULL; rp = rp->next)
    {
        Package *package = (Package *) rp->item;

        if (MatchPackage(pattern, package))
        {
            AppendRlistAlien(&matching, package);
        }
    }

    return matching;
}

/******************************************************************************/

static void ShowPackages(FILE *out, Rlist *package_entries)
{
    Rlist *rp = NULL;

    for (rp = package_entries; rp != NULL; rp = rp->next)
    {
        fprintf(out, "%s\n", SerializePackage((Package *) rp->item));
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

    Rlist *matching_available = FindPackages(AVAILABLE_PACKAGES_FILE_NAME, pattern);

    if (matching_available == NULL)
    {
        fprintf(stderr, "Unable to find any package matching %s\n", SerializePackagePattern(pattern));
        exit(1);
    }

    Rlist *rp;

    for (rp = matching_available; rp; rp = rp->next)
    {
        Package *p = (Package *) rp->item;

        PackagePattern *pat = MatchAllVersions((Package *) rp->item);

        if (FindPackages(INSTALLED_PACKAGES_FILE_NAME, pat) != NULL)
        {
            fprintf(stderr, "Package %s is already installed.\n", SerializePackage(p));
            exit(1);
        }
    }

    Rlist *installed_packages = ReadPackageEntries(INSTALLED_PACKAGES_FILE_NAME);

    for (rp = matching_available; rp; rp = rp->next)
    {
        Package *p = (Package *) rp->item;

        AppendRlistAlien(&installed_packages, p);
        fprintf(stderr, "Succesfully installed package %s\n", SerializePackage(p));
    }

    SavePackages(INSTALLED_PACKAGES_FILE_NAME, installed_packages);
    exit(0);
}

/******************************************************************************/

static void PopulateAvailable(const char *arg)
{
    Package *p = DeserializePackage(arg);
    PackagePattern *pattern = MatchSame(p);

    if (FindPackages(AVAILABLE_PACKAGES_FILE_NAME, pattern) != NULL)
    {
        fprintf(stderr, "Skipping already available package %s\n", SerializePackage(p));
        return;
    }

    Rlist *available_packages = ReadPackageEntries(AVAILABLE_PACKAGES_FILE_NAME);

    AppendRlistAlien(&available_packages, p);
    SavePackages(AVAILABLE_PACKAGES_FILE_NAME, available_packages);
}

/******************************************************************************/

int main(int argc, char *argv[])
{
    extern char *optarg;
    int option_index = 0;
    int c;

    char *workdir = getenv("CFENGINE_TEST_OVERRIDE_WORKDIR");

    snprintf(AVAILABLE_PACKAGES_FILE_NAME, 256,
             "%s/cfengine-mock-package-manager-available", workdir ? workdir : "/tmp");
    snprintf(INSTALLED_PACKAGES_FILE_NAME, 256,
             "%s/cfengine-mock-package-manager-installed", workdir ? workdir : "/tmp");

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
            Rlist *installed_packages = ReadPackageEntries(INSTALLED_PACKAGES_FILE_NAME);

            ShowPackages(stdout, installed_packages);
        }
            break;

        case 'L':
        {
            Rlist *available_packages = ReadPackageEntries(AVAILABLE_PACKAGES_FILE_NAME);

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
            Syntax("mock-package-manager - pretend that you are managing packages!", OPTIONS, HINTS, ID);
            exit(1);
        }

    }

    return 0;
}
