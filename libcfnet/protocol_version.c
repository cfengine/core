#include <platform.h>
#include <protocol_version.h>
#include <string_lib.h>

ProtocolVersion ParseProtocolVersionNetwork(const char *const s)
{
    int version;
    const int ret = sscanf(s, "CFE_v%d", &version);
    if (ret != 1 || version <= CF_PROTOCOL_UNDEFINED)
    {
        return CF_PROTOCOL_UNDEFINED;
    }
    // Note that `version` may be above CF_PROTOCOL_LATEST, if the other side
    // supports a newer protocol
    return version;
}

ProtocolVersion ParseProtocolVersionPolicy(const char *const s)
{
    if ((s == NULL) || StringEqual(s, "0") || StringEqual(s, "undefined"))
    {
        return CF_PROTOCOL_UNDEFINED;
    }
    if (StringEqual(s, "1") || StringEqual(s, "classic"))
    {
        return CF_PROTOCOL_CLASSIC;
    }
    else if (StringEqual(s, "2") || StringEqual(s, "tls"))
    {
        return CF_PROTOCOL_TLS;
    }
    else if (StringEqual(s, "3") || StringEqual(s, "cookie"))
    {
        return CF_PROTOCOL_COOKIE;
    }
    else if (StringEqual(s, "latest"))
    {
        return CF_PROTOCOL_LATEST;
    }
    else
    {
        return CF_PROTOCOL_UNDEFINED;
    }
}
