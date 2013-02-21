#include "cf3.defs.h"

#include "sysinfo.h"

#include <setjmp.h>
#include <cmockery.h>

/* Global variables we care about */

char VFQNAME[CF_MAXVARSIZE];
char VUQNAME[CF_MAXVARSIZE];
char VDOMAIN[CF_MAXVARSIZE];

/* */

static struct hostent h = {
    .h_name = "laptop.intra.cfengine.com"
};

int gethostname(char *name, size_t len)
{
    strcpy(name, "laptop.intra");
    return 0;
}

struct hostent *gethostbyname(const char *name)
{
    assert_string_equal(name, "laptop.intra");
    return &h;
}

typedef struct
{
    const char *name;
    bool found;
} ExpectedClasses;

ExpectedClasses expected_classes[] =
{
    {"laptop.intra.cfengine.com"},
    {"intra.cfengine.com"},
    {"cfengine.com"},
    {"com"},
    {"laptop.intra"},
};

void HardClass(const char *classname)
{
    int i;

    for (i = 0; i < sizeof(expected_classes) / sizeof(expected_classes[0]); ++i)        /* LCOV_EXCL_LINE */
    {
        if (!strcmp(classname, expected_classes[i].name))
        {
            expected_classes[i].found = true;
            return;
        }
    }
    fail();                     /* LCOV_EXCL_LINE */
}

typedef struct
{
    const char *name;
    const char *value;
    bool found;
} ExpectedVars;

ExpectedVars expected_vars[] =
{
    {"host", "laptop.intra"},
    {"fqhost", "laptop.intra.cfengine.com"},
    {"uqhost", "laptop.intra"},
    {"domain", "cfengine.com"},
};

void NewScalar(const char *namespace, const char *varname, const char *value, DataType type)
{
    int i;

    assert_string_equal(namespace, "sys");
    assert_int_equal(type, DATA_TYPE_STRING);

    for (i = 0; i < sizeof(expected_vars) / sizeof(expected_vars[0]); ++i)      /* LCOV_EXCL_LINE */
    {
        if (!strcmp(varname, expected_vars[i].name))
        {
            assert_string_equal(value, expected_vars[i].value);
            expected_vars[i].found = true;
            return;
        }
    }
    fprintf(stderr, "${%s.%s} <- %s (%c)\n", namespace, varname, value, type);  /* LCOV_EXCL_LINE */
    fail();                     /* LCOV_EXCL_LINE */
}

static void test_set_names(void **state)
{
    int i = 0;

    DetectDomainName("laptop.intra");

    for (i = 0; i < sizeof(expected_classes) / sizeof(expected_classes[0]); ++i)
    {
        assert_int_equal(expected_classes[i].found, true);
    }

    for (i = 0; i < sizeof(expected_vars) / sizeof(expected_vars[0]); ++i)
    {
        assert_int_equal(expected_vars[i].found, true);
    }
}

int main()
{
    const UnitTest tests[] =
{
        unit_test(test_set_names),
    };

    return run_tests(tests);
}

/* LCOV_EXCL_START */

/* Stub out functions we do not use in test */

int LOOKUP = false;

void __ProgrammingError(const char *file, int lineno, const char *format, ...)
{
    fail();
    exit(42);
}

void __UnexpectedError(const char *file, int lineno, const char *format, ...)
{
    fail();
}

void CfOut(OutputLevel level, const char *errstr, const char *fmt, ...)
{
    fail();
}

const char *NameVersion(void)
{
    fail();
}

int Unix_GetCurrentUserName(char *userName, int userNameLen)
{
    fail();
}

void Unix_FindV6InterfaceInfo(void)
{
    fail();
}

int cfstat(const char *path, struct stat *buf)
{
    fail();
}

void FatalError(char *s, ...)
{
    fail();
    exit(42);
}

void DeleteItemList(Item *item)
{
    fail();
}

Item *SplitString(const char *string, char sep)
{
    fail();
}

char *cf_ctime(const time_t *timep)
{
    fail();
}

char *CanonifyName(const char *str)
{
    fail();
}

void CanonifyNameInPlace(char *str)
{
    fail();
}

int FullTextMatch(const char *regptr, const char *cmpptr)
{
    fail();
}

const char *Version(void)
{
    fail();
}

const char *Nova_Version(void)
{
    fail();
}

char *Constellation_Version(void)
{
    fail();
}

void LoadSlowlyVaryingObservations(void)
{
    fail();
}

void HashPubKey(RSA *key, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type)
{
    fail();
}

char *MapName(char *s)
{
    fail();
}

char *HashPrint(HashMethod type, unsigned char digest[EVP_MAX_MD_SIZE + 1])
{
    fail();
}

void Unix_GetInterfaceInfo(AgentType ag)
{
    fail();
}

void EnterpriseContext(void)
{
    fail();
}

int StrnCmp(char *s1, char *s2, size_t n)
{
    fail();
}

ssize_t CfReadLine(char *buff, int size, FILE *fp)
{
    fail();
}

bool IsDefinedClass(const char *class)
{
    fail();
}

void DeleteVariable(const char *scope, const char *id)
{
    fail();
}

Rlist *RlistParseShown(char *string)
{
    fail();
}

void NewList(const char *scope, const char *lval, void *rval, DataType dt)
{
    fail();
}

void RlistDestroy(Rlist *list)
{
    fail();
}

/* Stub out variables */

int DEBUG;
AgentType THIS_AGENT_TYPE;
Item *IPADDRESSES;
struct utsname VSYSNAME;
PlatformContext VSYSTEMHARDCLASS;
char CFWORKDIR[CF_BUFSIZE];
char PUBKEY_DIGEST[CF_MAXVARSIZE];
HashMethod CF_DEFAULT_DIGEST;
char *CLASSATTRIBUTES[PLATFORM_CONTEXT_MAX][3];
const char *VFSTAB[1];
char *VRESOLVCONF[1];
char *VMAILDIR[1];
char *VEXPORTS[1];
char EXPIRY[CF_SMALLBUF];
RSA *PUBKEY;
const char *CLASSTEXT[1] = { };

char VIPADDRESS[18];

/* LCOV_EXCL_STOP */
