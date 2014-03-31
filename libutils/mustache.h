#ifndef CFENGINE_MUSTACHE_H
#define CFENGINE_MUSTACHE_H

#include <json.h>
#include <buffer.h>

bool MustacheRender(Buffer *out, const char *input, const JsonElement *hash);

#endif
