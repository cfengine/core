#include <logging.h>

void good(void)
{
    // Does include "MESSAGE"
    LogToSystemLogStructured(LOG_DEBUG, "FOO", "bogus", "BAR", "doofus", "MESSAGE", "bonkers");
}

void bad(void)
{
    // Does not include "MESSAGE"
    LogToSystemLogStructured(LOG_DEBUG, "FOO", "bogus", "BAR", "doofus", "BAZ", "bonkers");
}

int main()
{
    good();
    bad();
    return 0;
}
