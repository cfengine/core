#include <syntax.h>
#include <string.h>
#include <cfnet.h>
#include <sysinfo.h>

static const char* features[] = {
#ifdef HAVE_LIBYAML
  "yaml",
#endif
#ifdef HAVE_LIBXML2
  "xml2",
#endif

  NULL
};

int KnownFeature(const char *feature)
{
  // dumb algorithm, but still effective for a small number of features
  for(int i=0 ; features[i]!=NULL ; i++) {
    int r = strcmp(feature, features[i]);
    printf("%d: %s <-> %s\n", r, feature, features[i]);
    if(r==0) {
      return 1;
    }
  }
  return 0;
}

void CreateHardClassesFromfeatures(EvalContext *ctx, char *tags)
{
  char vbuff[CF_BUFSIZE];

  for(int i=0 ; features[i]!=NULL ; i++) {
    snprintf(vbuff, CF_BUFSIZE, "feature_%s", features[i]);
    CreateHardClassesFromCanonification(ctx, vbuff, tags);
  }
}
