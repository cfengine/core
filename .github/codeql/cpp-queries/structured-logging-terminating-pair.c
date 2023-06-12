#include <logging.h>

void good(void)
{
    // Does include "MESSAGE"
    LogToSystemLogStructured(LOG_DEBUG, "FOO", "bogus", "BAR", "doofus", "MESSAGE", "%s!", "bonkers");
}

void bad(void)
{
    // Does not include "MESSAGE"
    LogToSystemLogStructured(LOG_DEBUG, "FOO", "bogus", "BAR", "doofus", "BAZ", "%s!", "bonkers");
}

int main()
{
    good();
    bad();
    return 0;
}
