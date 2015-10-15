#include <syntax.h>

#include <string.h>

static const char* features[] = {
// the parser leaves a trailing ')' 
#ifdef HAVE_LIBYAML
  "yaml)",
#endif
#ifdef HAVE_LIBXML2
  "xml2)",
#endif

  ")" // This terminates the array and declares empty as an always existing feature
};

int KnownFeature(const char *feature)
{
  int feature_count = sizeof(features)/sizeof(const char*);
  char copy[200];

  // dumb algorithm, but still effective for a small number of features
  for(int i=0 ; i<feature_count ; i++) {
    int r = strcmp(feature, features[i]);
    printf("%d: %s <-> %s\n", r, feature, features[i]);
    if(r==0) {
      return 1;
    }
  }
  return 0;
}

