#include "cf3.defs.h"
#include "cf3.extern.h"

#define DB "db.db"
#define ATTEMPTS 1000

void *contend(void *param)
{
CF_TCDB* db;
int i;
for (i = 0; i < ATTEMPTS; ++i)
   {
   if (!TCDB_OpenDB(DB, &db))
      {
      exit(42);
      }
   else
      {
      TCDB_CloseDB(db);
      }
   }
return NULL;
}

int main(int argc, char **argv)
{
pthread_attr_t attr;
pthread_attr_init(&attr);
pthread_attr_setstacksize(&attr, 65536);
pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

int numthreads = atoi(argv[1]);

int i;
for (i = 0; i < numthreads; ++i)
   {
   pthread_t tid;

   void *p = (void*)i;

   int ret = pthread_create(&tid, &attr, &contend, p);
   if (ret != 0)
      {
      fprintf(stderr, "Unable to create thread: %s\n", strerror(ret));
      }
   }

pthread_attr_destroy(&attr);
pthread_exit(NULL);
}

/* Stub out */

void CfOut(enum cfreport level, const char *function, const char* fmt, ...)
{
exit(42);
}

void FatalError(char *fmt, ...)
{
exit(42);
}

int D1;
int D2;
int DEBUG;
