#include "cf3.defs.h"
#include "dbm_api.h"
#include "lastseen.h"

#include "alphalist.h"

char CFWORKDIR[CF_BUFSIZE] = "/tmp";


void UpdateLastSawHost(const char *hostkey, const char *address,
                       bool incoming, time_t timestamp);

int main()
{
    snprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/lastseen_migration_test.XXXXXX");
    mkdtemp(CFWORKDIR);

    for (int i = 0; i < 1000000; ++i)
    {
        if ((i % 10000) == 0)
        {
            printf(".");
            fflush(stdout);
        }

        char hostkey[50];
        snprintf(hostkey, 50, "SHA-%040d", i);
        char ip[50];
        snprintf(ip, 50, "250.%03d.%03d.%03d", i / (256*256), (i / 256) % 256, i % 256);

        UpdateLastSawHost(hostkey, ip, false, i);
        UpdateLastSawHost(hostkey, ip, true, 2000000 - i);
    }

    char cmd[CF_BUFSIZE];
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);

    return 0;
}

/* STUBS */

void __ProgrammingError(const char *file, int lineno, const char *format, ...)
{
    exit(42);
}

void FatalError(char *s, ...)
{
    exit(42);
}

void CfOut(OutputLevel level, const char *errstr, const char *fmt, ...)
{
    fprintf(stderr, "CFOUT<%d>: ", level);
    if (errstr)
    {
        fprintf(stderr, " %s: %s ", errstr, strerror(errno));
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

HashMethod CF_DEFAULT_DIGEST;
const char *DAY_TEXT[] = {};
const char *MONTH_TEXT[] = {};
const char *SHIFT_TEXT[] = {};
pthread_mutex_t *cft_output;
char VIPADDRESS[18];
RSA *PUBKEY;

AlphaListIterator AlphaListIteratorInit(AlphaList *al)
{
    exit(42);
}

const Item *AlphaListIteratorNext(AlphaListIterator *iterator)
{
    exit(42);
}

Item *IdempPrependItem(Item **liststart, const char *itemstring, const char *classes)
{
    exit(42);
}

bool IsItemIn(Item *list, const char *item)
{
    exit(42);
}

int DEBUG;

void DeleteItemList(Item *item)
{
    exit(42);
}

bool MINUSF;

char *MapAddress(char *addr)
{
    exit(42);
}

char *HashPrint(HashMethod type, unsigned char digest[EVP_MAX_MD_SIZE + 1])
{
    exit(42);
}

int ThreadLock(pthread_mutex_t *name)
{
    exit(42);
}

int ThreadUnlock(pthread_mutex_t *name)
{
    exit(42);
}

void HashPubKey(RSA *key, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type)
{
    exit(42);
}

void *ConstraintGetRvalValue(char *lval, Promise *promise, char type)
{
    exit(42);
}
