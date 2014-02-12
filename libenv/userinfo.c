#include <userinfo.h>

#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>

#include <eval_context.h>

void GetCurrentUserInfo(EvalContext *ctx)
{
    struct passwd *pw;
    if (( pw = getpwuid(getuid())) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to get username for uid '%ju'. (getpwuid: %s)", (uintmax_t)getuid(), GetErrorStr());
    }
    else
    {
        char buf[CF_BUFSIZE];

        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_USER, "name", pw->pw_name, CF_DATA_TYPE_STRING, "source=agent");
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_USER, "homedir", pw->pw_dir, CF_DATA_TYPE_STRING, "source=agent");
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_USER, "shell", pw->pw_shell, CF_DATA_TYPE_STRING, "source=agent");

        snprintf(buf, CF_BUFSIZE, "%i", pw->pw_uid);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_USER, "uid", buf, CF_DATA_TYPE_INT, "source=agent");

        snprintf(buf, CF_BUFSIZE, "%i", pw->pw_gid);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_USER, "gid", buf, CF_DATA_TYPE_INT, "source=agent");
    }
}
