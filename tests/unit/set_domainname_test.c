#include "cf3.defs.h"

#include "sysinfo.h"
#include "env_context.h"
#include "item_lib.h"

#include "test.h"

/* Global variables we care about */

char VFQNAME[CF_MAXVARSIZE];
char VUQNAME[CF_MAXVARSIZE];
char VDOMAIN[CF_MAXVARSIZE];

/* */

static struct hostent h = {
    .h_name = "laptop.intra.cfengine.com"
};

#ifdef SOLARIS
int gethostname(char *name, int len)
#else
int gethostname(char *name, size_t len)
#endif
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

void EvalContextHeapAddHard(EvalContext *ctx, const char *classname)
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

void ScopeNewScalar(EvalContext *ctx, const char *ns, const char *varname, const char *value, DataType type)
{
    int i;

    assert_string_equal(ns, "sys");
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
    fprintf(stderr, "${%s.%s} <- %s (%c)\n", ns, varname, value, type);  /* LCOV_EXCL_LINE */
    fail();                     /* LCOV_EXCL_LINE */
}

void ScopeNewSpecialScalar(EvalContext *ctx, const char *ns, const char *varname, const char *value, DataType type)
{
    ScopeNewScalar(ctx, ns, varname, value, type);
}

static void test_set_names(void)
{
    int i = 0;

    EvalContext *ctx = EvalContextNew();
    DetectDomainName(ctx, "laptop.intra");

    for (i = 0; i < sizeof(expected_classes) / sizeof(expected_classes[0]); ++i)
    {
        assert_int_equal(expected_classes[i].found, true);
    }

    for (i = 0; i < sizeof(expected_vars) / sizeof(expected_vars[0]); ++i)
    {
        assert_int_equal(expected_vars[i].found, true);
    }
    EvalContextDestroy(ctx);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_set_names),
    };

    return run_tests(tests);
}

/* LCOV_EXCL_START */

/* Stub out functions we do not use in test */

int LOOKUP = false;

EvalContext *EvalContextNew(void)
{
    EvalContext *ctx = xmalloc(sizeof(EvalContext));

    ctx->heap_soft = StringSetNew();
    ctx->heap_hard = StringSetNew();

    return ctx;
}

void EvalContextDestroy(EvalContext *ctx)
{
    if (ctx)
    {
        StringSetDestroy(ctx->heap_soft);
        StringSetDestroy(ctx->heap_hard);
    }
}

void DeleteItemList(Item *item) /* delete starting from item */
{
    Item *ip, *next;

    for (ip = item; ip != NULL; ip = next)
    {
        next = ip->next;        // save before free

        if (ip->name != NULL)
        {
            free(ip->name);
        }

        if (ip->classes != NULL)
        {
            free(ip->classes);
        }

        free((char *) ip);
    }
}

void __ProgrammingError(const char *file, int lineno, const char *format, ...)
{
    fail();
    exit(42);
}

void __UnexpectedError(const char *file, int lineno, const char *format, ...)
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

void FatalError(char *s, ...)
{
    fail();
    exit(42);
}

Item *SplitString(const char *string, char sep)
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

void LoadSlowlyVaryingObservations(EvalContext *ctx)
{
    fail();
}

void HashPubKey(RSA *key, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type)
{
    fail();
}

char *HashPrintSafe(HashMethod type, unsigned char digest[EVP_MAX_MD_SIZE + 1], char buffer[EVP_MAX_MD_SIZE * 4])
{
    fail();
}

void Unix_GetInterfaceInfo(AgentType ag)
{
    fail();
}

void EnterpriseContext(EvalContext *ctx)
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

bool IsDefinedClass(const EvalContext *ctx, const char *class, const char *ns)
{
    fail();
}

void ScopeDeleteSpecialScalar(const char *scope, const char *id)
{
    fail();
}

void ScopeDeleteVariable(const char *scope, const char *id)
{
    fail();
}


Rlist *RlistParseShown(char *string)
{
    fail();
}

void ScopeNewList(EvalContext *ctx, const char *scope, const char *lval, void *rval, DataType dt)
{
    fail();
}

void ScopeNewSpecialList(EvalContext *ctx, const char *scope, const char *lval, void *rval, DataType dt)
{
    fail();
}

void RlistDestroy(Rlist *list)
{
    fail();
}

/* Stub out variables */

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

char VIPADDRESS[CF_MAX_IP_LEN];

/* LCOV_EXCL_STOP */
