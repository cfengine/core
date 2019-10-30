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
    if ((s == NULL) || StringSafeEqual(s, "0") || StringSafeEqual(s, "undefined"))
    {
        return CF_PROTOCOL_UNDEFINED;
    }
    if (StringSafeEqual(s, "1") || StringSafeEqual(s, "classic"))
    {
        return CF_PROTOCOL_CLASSIC;
    }
    else if (StringSafeEqual(s, "2") || StringSafeEqual(s, "tls"))
    {
        return CF_PROTOCOL_TLS;
    }
    else if (StringSafeEqual(s, "latest"))
    {
        return CF_PROTOCOL_LATEST;
    }
    else
    {
        return CF_PROTOCOL_UNDEFINED;
    }
}
