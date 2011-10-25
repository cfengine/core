#include "cf3.defs.h"

static const char *REPOSITORY_PACKAGES_FILE_NAME = "/tmp/cfengine-mock-package-manager-repository";
static const char *INSTALLED_PACKAGES_FILE_NAME = "/tmp/cfengine-mock-package-manager-installed";
static const int MAX_PACKAGE_ENTRY_LENGTH = 256;

#define Error(msg) fprintf(stderr, "%s:%d: %s", __FILE__, __LINE__, msg)

const char *ID = "The CFEngine mock package manager tricks cf-agent into thinking\n"
		 "it has actually installed something on the system. Knock yourself out!";

const struct option OPTIONS[] =
      {
      { "list-installed",no_argument,0,'l' },
      { "list-available",no_argument,0,'L' },
      { "add",required_argument,0,'a' },
      { "delete", required_argument, 0, 'd' },
      { "reinstall", required_argument, 0, 'r'},
      { "update", required_argument, 0, 'u'},
      { "addupdate", required_argument, 0, 'U'},
      { "verify", required_argument, 0, 'v'},
      { NULL,0,0,'\0' }
      };

const char *HINTS[] =
      {
      "Add an imaginary package",
      "Delete a previously imagined package",
      "Reinstall an imaginary package",
      "Update a previously imagined package",
      "Add or update an imaginary package",
      "Verify a previously imagined package",
      NULL
      };

struct Package
   {
   char *name;
   char *version;
   char *arch;
   };

static char *SerializePackage(struct Package *package)
{
char *entry = xcalloc(MAX_PACKAGE_ENTRY_LENGTH, sizeof(char));
snprintf(entry, MAX_PACKAGE_ENTRY_LENGTH, "%s:%s:%s",
      package->name, package->version, package->arch);
return entry;
}

static struct Package *DeserializePackage(const char *entry)
{
struct Package *package = xcalloc(1, sizeof(struct Package));
char *entry_copy = NULL;

if (entry == NULL)
   {
   return NULL;
   }

entry_copy = xcalloc(strlen(entry), sizeof(char));
strcpy(entry_copy, entry);

package->name = strtok(entry_copy, ":");
package->version = strtok(NULL, ":");
package->arch = strtok(NULL, ":");

return package;
}

static struct Rlist *ReadPackageEntries(const char *database_filename)
{
FILE *packages_file = fopen(database_filename, "r");

if (packages_file != NULL)
   {
   struct Rlist *packages = NULL;
   char serialized_package[MAX_PACKAGE_ENTRY_LENGTH];

   while (fscanf(packages_file, "%s\n", serialized_package) != EOF)
      {
      AppendRlist(&packages, serialized_package, CF_SCALAR);
      }

   fclose(packages_file);
   return packages;
   }

return NULL;
}

static void SavePackages(const char *database_filename, struct Rlist *package_entries)
{
struct Rlist *rp = NULL;
FILE *packages_file = fopen(database_filename, "w");

for (rp = package_entries; rp != NULL; rp = rp->next)
   {
   fprintf(packages_file, "%s\n", (char *)rp->item);
   }

fclose(packages_file);
}

static bool IsPackageEqual(struct Package *a, struct Package *b)
{
return strcmp(a->name, b->name) == 0 &&
       strcmp(a->version, b->version) == 0 &&
       strcmp(a->arch, b->arch) == 0;
}

static struct Package *FindPackage(const char *database_filename, struct Package *query_package)
{
struct Rlist *available = NULL;
struct Rlist *rp = NULL;

available = ReadPackageEntries(database_filename);

for (rp = available; rp != NULL; rp = rp->next)
   {
   struct Package *package = DeserializePackage((const char *)rp->item);

   if (IsPackageEqual(package, query_package))
      {
      DeleteRlist(available);
      return package;
      }
   else
      {
      free(package);
      }
   }

DeleteRlist(available);
return false;
}

static void ShowPackages(FILE* out, struct Rlist *package_entries)
{
struct Rlist *rp = NULL;

for (rp = package_entries; rp != NULL; rp = rp->next)
   {
   fprintf(out, "%s\n", (char *)rp->item);
   }
}

static void AddPackage(struct Package *package)
{
if (FindPackage(INSTALLED_PACKAGES_FILE_NAME, package) == NULL)
   {
   struct Rlist *installed_packages = ReadPackageEntries(INSTALLED_PACKAGES_FILE_NAME);
   char *package_entry = SerializePackage(package);

   AppendRlist(&installed_packages, package_entry, CF_SCALAR);
   SavePackages(INSTALLED_PACKAGES_FILE_NAME, installed_packages);

   free(package_entry);
   DeleteRlist(installed_packages);
   }
}

static void DeletePackage(struct Package *query_package)
{
struct Rlist *delete_entry = NULL;
struct Rlist *installed_package_entries = ReadPackageEntries(INSTALLED_PACKAGES_FILE_NAME);

   {
   char *serialized_query_package = SerializePackage(query_package);
   delete_entry = KeyInRlist(installed_package_entries, serialized_query_package);
   free(serialized_query_package);
   }

DeleteRlistEntry(&installed_package_entries, delete_entry);

SavePackages(INSTALLED_PACKAGES_FILE_NAME, installed_package_entries);
}

static void ReinstallPackage(struct Package *package)
{
AddPackage(package);
}

static void UpdatePackage(struct Package *package)
{
}

static void AddUpdatePackage(struct Package *package)
{

}

static void VerifyPackage(struct Package *package)
{

}

int main(int argc, char *argv[])
{
extern char *optarg;
int option_index = 0;
char c = '\0';
char arg[CF_BUFSIZE];

while ((c = getopt_long(argc, argv, "rd:vnKIf:D:N:Vs:x:MBb:", OPTIONS, &option_index)) != EOF)
   {
   struct Package *package = DeserializePackage(optarg);

   switch (c)
      {
      case 'l':
	 {
	 struct Rlist *installed_packages = ReadPackageEntries(INSTALLED_PACKAGES_FILE_NAME);
	 ShowPackages(stdout, installed_packages);
	 DeleteRlist(installed_packages);
	 }
	 break;

      case 'L':
	 {
	 struct Rlist *available_packages = ReadPackageEntries(REPOSITORY_PACKAGES_FILE_NAME);
	 ShowPackages(stdout, available_packages);
	 DeleteRlist(available_packages);
	 }
	 break;

      case 'a':
	 AddPackage(package);
	 break;

      case 'd':
      	 DeletePackage(package);
      	 break;

      case 'r':
	 ReinstallPackage(package);
	 break;

      case 'u':
	 UpdatePackage(package);
	 break;

      case 'U':
	 AddUpdatePackage(package);
	 break;

      case 'v':
      	 VerifyPackage(package);
      	 break;

      default:
	 Syntax("mock-package-manager - pretend that you are managing packages!",OPTIONS,HINTS,ID);
         exit(1);
      }

   }

return 0;
}
