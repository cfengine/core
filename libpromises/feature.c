#include <syntax.h>
#include <string.h>
#include <cfnet.h>
#include <sysinfo.h>
#include <buffer.h>

static const char* features[] = {
#ifdef HAVE_LIBYAML
    "yaml",
#endif
#ifdef HAVE_LIBXML2
    "xml",
#endif
#ifdef HAVE_LIBCURL
    "curl",
#endif
    "tls_1_0",                  /* we require versions of OpenSSL that support
                                 * at least TLS 1.0 */
#ifdef HAVE_TLS_1_1
    "tls_1_1",
#endif
#ifdef HAVE_TLS_1_2
    "tls_1_2",
#endif
#ifdef HAVE_TLS_1_3
    "tls_1_3",
#endif
    "def_json_preparse",
    NULL
};

int KnownFeature(const char *feature)
{
    // dumb algorithm, but still effective for a small number of features
    for(int i=0 ; features[i]!=NULL ; i++) {
        int r = strcmp(feature, features[i]);
        if(r==0) {
            return 1;
        }
    }
    return 0;
}

void CreateHardClassesFromFeatures(EvalContext *ctx, char *tags)
{
    Buffer *buffer = BufferNew();

    for(int i=0 ; features[i]!=NULL ; i++) {
        BufferPrintf(buffer, "feature_%s", features[i]);
        CreateHardClassesFromCanonification(ctx, BufferData(buffer), tags);
    }
    BufferDestroy(buffer);
}
