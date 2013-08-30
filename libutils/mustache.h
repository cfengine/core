#ifndef CFENGINE_MUSTACHE_H
#define CFENGINE_MUSTACHE_H

#include <json.h>
#include <writer.h>

bool MustacheRender(Writer *out, const char *input, const JsonElement *hash);

#endif
