#include "test.h"
#include "evalfunction.h"

static bool netgroup_more = false;

int setnetgrent(const char *netgroup)
{
   if (strcmp(netgroup, "valid_netgroup") == 0)
      {
      netgroup_more = true;
      return 1;
      }
   netgroup_more = false;
   return 0;
}

int getnetgrent (char **hostp, char **userp, char **domainp)
{
   if (netgroup_more)
      {
      *hostp = NULL;
      *userp = "user";
      *domainp = NULL;

      netgroup_more = false;
      return 1;
      }
   else
      {
      return 0;
      }
}

static void test_hostinnetgroup_found(void **state)
{
FnCallResult res;
struct Rlist *args = NULL;
AppendRlist(&args, "valid_netgroup", 's');

res = FnCallHostInNetgroup(NULL, args);
assert_string_equal("any", (char *)res.rval.item);
}

static void test_hostinnetgroup_not_found(void **state)
{
FnCallResult res;
struct Rlist *args = NULL;
AppendRlist(&args, "invalid_netgroup", 's');

res = FnCallHostInNetgroup(NULL, args);
assert_string_equal("!any", (char *)res.rval.item);
}

int main()
{
const UnitTest tests[] =
   {
   unit_test(test_hostinnetgroup_found),
   unit_test(test_hostinnetgroup_not_found),
   };

return run_tests(tests);
}

