#include <platform.h>
#include <protocol_version.h>

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
    if ((s == NULL) || (strcmp(s, "0") == 0) || (strcmp(s, "undefined") == 0))
    {
        return CF_PROTOCOL_UNDEFINED;
    }
    if ((strcmp(s, "1") == 0) || (strcmp(s, "classic") == 0))
    {
        return CF_PROTOCOL_CLASSIC;
    }
    else if ((strcmp(s, "2") == 0) || (strcmp(s, "tls") == 0))
    {
        return CF_PROTOCOL_TLS;
    }
    else if (strcmp(s, "latest") == 0)
    {
        return CF_PROTOCOL_LATEST;
    }
    else
    {
        return CF_PROTOCOL_UNDEFINED;
    }
}
