#include <mustache.h>

#include <string_lib.h>
#include <logging.h>
#include <alloc.h>
#include <sequence.h>

typedef enum
{
    TAG_TYPE_VAR,
    TAG_TYPE_VAR_UNESCAPED,
    TAG_TYPE_SECTION,
    TAG_TYPE_SECTION_END,
    TAG_TYPE_INVERTED,
    TAG_TYPE_COMMENT,
    TAG_TYPE_DELIM,
    TAG_TYPE_ERR,
    TAG_TYPE_NONE
} TagType;

typedef struct
{
    TagType type;
    const char *begin;
    const char *end;
    const char *content;
    size_t content_len;
} Mustache;

#define MUSTACHE_MAX_DELIM_SIZE 10

static bool IsSpace(char c)
{
    return c == '\t' || c == ' ';
}

static bool IsTagStandalone(const char *start, const char *tag_start, const char *tag_end, const char **line_begin, const char **line_end)
{
    assert(start <= tag_start);

    *line_begin = start;
    for (const char *cur = tag_start - 1; cur >= start; cur--)
    {
        if (IsSpace(*cur))
        {
            *line_begin = cur;
            if (cur == start)
            {
                break;
            }
            continue;
        }
        else if (*cur == '\n')
        {
            *line_begin = cur + 1;
            break;
        }
        else
        {
            return false;
        }
    }

    *line_end = NULL;
    for (const char *cur = tag_end; true; cur++)
    {
        if (IsSpace(*cur))
        {
            continue;
        }
        else if (*cur == '\n')
        {
            *line_end = cur + 1;
            break;
        }
        else if (*cur == '\r')
        {
            if (*(cur + 1) == '\n')
            {
                *line_end = cur + 2;
                break;
            }
            continue;
        }
        else if (*cur == '\0')
        {
            *line_end = cur;
            break;
        }
        else
        {
            return false;
        }
    }

    assert(*line_end);

    return true;
}

static bool IsTagTypeRenderable(TagType type)
{
    switch (type)
    {
    case TAG_TYPE_COMMENT:
    case TAG_TYPE_DELIM:
    case TAG_TYPE_ERR:
    case TAG_TYPE_INVERTED:
    case TAG_TYPE_SECTION:
    case TAG_TYPE_SECTION_END:
        return false;
    default:
        return true;
    }
}

static JsonElement *LookupVariable(Seq *hash_stack, const char *name, size_t name_len)
{
    assert(SeqLength(hash_stack) > 0);

    size_t num_comps = StringCountTokens(name, name_len, ".");

    JsonElement *base_var = NULL;
    {
        StringRef base_comp = StringGetToken(name, name_len, 0, ".");
        char *base_comp_str = xstrndup(base_comp.data, base_comp.len);

        for (ssize_t i = SeqLength(hash_stack) - 1; i >= 0; i--)
        {
            JsonElement *hash = SeqAt(hash_stack, i);
            if (!hash)
            {
                continue;
            }

            if (JsonGetElementType(hash) == JSON_ELEMENT_TYPE_CONTAINER && JsonGetContainerType(hash) == JSON_CONTAINER_TYPE_OBJECT)
            {
                JsonElement *var = JsonObjectGet(hash, base_comp_str);
                if (var)
                {
                    base_var = var;
                    break;
                }
            }
        }
        free(base_comp_str);
    }

    if (!base_var)
    {
        return NULL;
    }

    for (size_t i = 1; i < num_comps; i++)
    {
        if (JsonGetElementType(base_var) != JSON_ELEMENT_TYPE_CONTAINER || JsonGetContainerType(base_var) != JSON_CONTAINER_TYPE_OBJECT)
        {
            return NULL;
        }

        StringRef comp = StringGetToken(name, name_len, i, ".");
        char *comp_str = xstrndup(comp.data, comp.len);
        base_var = JsonObjectGet(base_var, comp_str);
        free(comp_str);

        if (!base_var)
        {
            return NULL;
        }
    }

    assert(base_var);
    return base_var;
}

static Mustache NextTag(const char *input,
                        const char *delim_start, size_t delim_start_len,
                        const char *delim_end, size_t delim_end_len)
{
    Mustache ret;
    ret.type = TAG_TYPE_NONE;

    ret.begin = strstr(input, delim_start);
    if (!ret.begin)
    {
        return ret;
    }

    ret.content = ret.begin + delim_start_len;
    char *extra_end = NULL;

    switch (ret.content[0])
    {
    case '#':
        ret.type = TAG_TYPE_SECTION;
        ret.content++;
        break;
    case '^':
        ret.type = TAG_TYPE_INVERTED;
        ret.content++;
        break;
    case '/':
        ret.type = TAG_TYPE_SECTION_END;
        ret.content++;
        break;
    case '!':
        ret.type = TAG_TYPE_COMMENT;
        ret.content++;
        break;
    case '=':
        extra_end = "=";
        ret.type = TAG_TYPE_DELIM;
        ret.content++;
        break;
    case '{':
        extra_end = "}";
    case '&':
        ret.type = TAG_TYPE_VAR_UNESCAPED;
        ret.content++;
        break;
    default:
        ret.type = TAG_TYPE_VAR;
        break;
    }

    if (extra_end)
    {
        const char *escape_end = strstr(ret.content, extra_end);
        if (!escape_end || strncmp(escape_end + 1, delim_end, delim_end_len) != 0)
        {
            Log(LOG_LEVEL_WARNING, "Broken mustache template, couldn't find end tag for quoted begin tag at '%20s'...", input);
            ret.type = TAG_TYPE_ERR;
            return ret;
        }

        ret.content_len = escape_end - ret.content;
        ret.end = escape_end + 1 + delim_end_len;
    }
    else
    {
        ret.end = strstr(ret.content, delim_end);
        if (!ret.end)
        {
            Log(LOG_LEVEL_WARNING, "Broken Mustache template, could not find end delimiter after reading start delimiter at '%20s'...", input);
            ret.type = TAG_TYPE_ERR;
            return ret;
        }

        ret.content_len = ret.end - ret.content;
        ret.end += delim_end_len;
    }

    while (*ret.content == ' ' || *ret.content == '\t')
    {
        ret.content++;
        ret.content_len--;
    }

    while (ret.content[ret.content_len - 1] == ' ' || ret.content[ret.content_len - 1] == '\t')
    {
        ret.content_len--;
    }

    return ret;
}

static void RenderHTMLContent(Buffer *out, const char *input, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        switch (input[i])
        {
        case '&':
            BufferAppendString(out, "&amp;");
            break;

        case '"':
            BufferAppendString(out, "&quot;");
            break;

        case '<':
            BufferAppendString(out, "&lt;");
            break;

        case '>':
            BufferAppendString(out, "&gt;");
            break;

        default:
            BufferAppendChar(out, input[i]);
            break;
        }
    }
}

static void RenderContent(Buffer *out, const char *content, size_t len, bool html, bool skip_content)
{
    if (skip_content)
    {
        return;
    }

    if (html)
    {
        RenderHTMLContent(out, content, len);
    }
    else
    {
        BufferAppend(out, content, len);
    }
}

static bool RenderVariablePrimitive(Buffer *out, const JsonElement *primitive, const bool escaped)
{
    switch (JsonGetPrimitiveType(primitive))
    {
    case JSON_PRIMITIVE_TYPE_STRING:
        if (escaped)
        {
            RenderHTMLContent(out, JsonPrimitiveGetAsString(primitive), strlen(JsonPrimitiveGetAsString(primitive)));
        }
        else
        {
            BufferAppendString(out, JsonPrimitiveGetAsString(primitive));
        }
        return true;

    case JSON_PRIMITIVE_TYPE_INTEGER:
        {
            char *str = StringFromLong(JsonPrimitiveGetAsInteger(primitive));
            BufferAppendString(out, str);
            free(str);
        }
        return true;

    case JSON_PRIMITIVE_TYPE_REAL:
        {
            char *str = StringFromDouble(JsonPrimitiveGetAsReal(primitive));
            BufferAppendString(out, str);
            free(str);
        }
        return true;

    case JSON_PRIMITIVE_TYPE_BOOL:
        BufferAppendString(out, JsonPrimitiveGetAsBool(primitive) ? "true" : "false");
        return true;

    case JSON_PRIMITIVE_TYPE_NULL:
        return true;

    default:
        assert(!"Unrecognised JSON primitive type");
    }
    return false;
}

static bool RenderVariable(Buffer *out,
                           const char *content, size_t content_len,
                           bool escaped,
                           Seq *hash_stack)
{
    JsonElement *var = NULL;
    if (strncmp(content, ".", content_len) == 0)
    {
        var = SeqAt(hash_stack, SeqLength(hash_stack) - 1);
    }
    else
    {
        var = LookupVariable(hash_stack, content, content_len);
    }

    if (!var)
    {
        return true;
    }

    switch (JsonGetElementType(var))
    {
    case JSON_ELEMENT_TYPE_PRIMITIVE:
        return RenderVariablePrimitive(out, var, escaped);

    case JSON_ELEMENT_TYPE_CONTAINER:
        assert(false);
        return false;
    }

    assert(false);
    return false;
}

static bool SetDelimiters(const char *content, size_t content_len,
                          char *delim_start, size_t *delim_start_len,
                          char *delim_end, size_t *delim_end_len)
{
    size_t num_tokens = StringCountTokens(content, content_len, " \t");
    if (num_tokens != 2)
    {
        Log(LOG_LEVEL_WARNING, "Could not parse delimiter mustache, number of tokens is %zd, expected 2 in '%s'",
            num_tokens, content);
        return false;
    }

    StringRef first = StringGetToken(content, content_len, 0, " \t");
    if (first.len > MUSTACHE_MAX_DELIM_SIZE)
    {
        Log(LOG_LEVEL_WARNING, "New mustache start delimiter exceeds the allowed size of %d in '%s'",
            MUSTACHE_MAX_DELIM_SIZE, content);
        return false;
    }
    strncpy(delim_start, first.data, first.len);
    delim_start[first.len] = '\0';
    *delim_start_len = first.len;

    StringRef second = StringGetToken(content, content_len, 1, " \t");
    if (second.len > MUSTACHE_MAX_DELIM_SIZE)
    {
        Log(LOG_LEVEL_WARNING, "New mustache start delimiter exceeds the allowed size of %d in '%s'",
            MUSTACHE_MAX_DELIM_SIZE, content);
        return false;
    }
    strncpy(delim_end, second.data, second.len);
    delim_end[second.len] = '\0';
    *delim_end_len = second.len;

    return true;
}

static bool Render(Buffer *out, const char *start, const char *input, Seq *hash_stack,
                   char *delim_start, size_t *delim_start_len,
                   char *delim_end, size_t *delim_end_len,
                   bool skip_content,
                   const char *section,
                   const char **section_end)
{
    while (true)
    {
        if (!input)
        {
            Log(LOG_LEVEL_ERR, "Unexpected end to Mustache template");
            return false;
        }

        Mustache tag = NextTag(input, delim_start, *delim_start_len, delim_end, *delim_end_len);

        {
            const char *line_begin = NULL;
            const char *line_end = NULL;
            if (!IsTagTypeRenderable(tag.type) && IsTagStandalone(start, tag.begin, tag.end, &line_begin, &line_end))
            {
                RenderContent(out, input, line_begin - input, false, skip_content);
                input = line_end;
            }
            else
            {
                RenderContent(out, input, tag.begin - input, false, skip_content);
                input = tag.end;
            }
        }

        switch (tag.type)
        {
        case TAG_TYPE_ERR:
            return false;

        case TAG_TYPE_DELIM:
            if (!SetDelimiters(tag.content, tag.content_len,
                               delim_start, delim_start_len,
                               delim_end, delim_end_len))
            {
                return false;
            }
            continue;

        case TAG_TYPE_COMMENT:
            continue;

        case TAG_TYPE_NONE:
            return true;

        case TAG_TYPE_VAR_UNESCAPED:
        case TAG_TYPE_VAR:
            if (!skip_content)
            {
                if (tag.content_len > 0)
                {
                    if (!RenderVariable(out, tag.content, tag.content_len, tag.type == TAG_TYPE_VAR, hash_stack))
                    {
                        return false;
                    }
                }
                else
                {
                    RenderContent(out, delim_start, *delim_start_len, false, false);
                    RenderContent(out, delim_end, *delim_end_len, false, false);
                }
            }
            continue;

        case TAG_TYPE_INVERTED:
        case TAG_TYPE_SECTION:
            {
                char *section = xstrndup(tag.content, tag.content_len);
                JsonElement *var = LookupVariable(hash_stack, tag.content, tag.content_len);
                SeqAppend(hash_stack, var);

                if (!var)
                {
                    const char *cur_section_end = NULL;
                    if (!Render(out, start, input, hash_stack, delim_start, delim_start_len, delim_end, delim_end_len,
                                skip_content || tag.type != TAG_TYPE_INVERTED, section, &cur_section_end))
                    {
                        free(section);
                        return false;
                    }
                    free(section);
                    input = cur_section_end;
                    continue;
                }

                switch (JsonGetElementType(var))
                {
                case JSON_ELEMENT_TYPE_PRIMITIVE:
                    switch (JsonGetPrimitiveType(var))
                    {
                    case JSON_PRIMITIVE_TYPE_BOOL:
                        {
                            bool skip = skip_content || (!JsonPrimitiveGetAsBool(var) ^ (tag.type == TAG_TYPE_INVERTED));

                            const char *cur_section_end = NULL;
                            if (!Render(out, start, input, hash_stack, delim_start, delim_start_len, delim_end, delim_end_len,
                                        skip, section, &cur_section_end))
                            {
                                free(section);
                                return false;
                            }
                            free(section);
                            input = cur_section_end;
                        }
                        continue;

                    default:
                        Log(LOG_LEVEL_WARNING, "Mustache sections can only take a boolean or a container (array or map) value, but section '%s' isn't getting one of those.",
                            section);
                        return false;
                    }
                    break;

                case JSON_ELEMENT_TYPE_CONTAINER:
                    switch (JsonGetContainerType(var))
                    {
                    case JSON_CONTAINER_TYPE_OBJECT:
                        {
                            const char *cur_section_end = NULL;
                            if (!Render(out, start, input,
                                        hash_stack,
                                        delim_start, delim_start_len, delim_end, delim_end_len,
                                        skip_content || tag.type == TAG_TYPE_INVERTED, section, &cur_section_end))
                            {
                                free(section);
                                return false;
                            }
                            free(section);
                            input = cur_section_end;
                        }
                        break;

                    case JSON_CONTAINER_TYPE_ARRAY:
                        if (JsonLength(var) > 0)
                        {
                            const char *cur_section_end = NULL;
                            for (size_t i = 0; i < JsonLength(var); i++)
                            {
                                JsonElement *child_hash = JsonAt(var, i);
                                SeqAppend(hash_stack, child_hash);


                                if (!Render(out, start, input,
                                            hash_stack,
                                            delim_start, delim_start_len, delim_end, delim_end_len,
                                            skip_content || tag.type == TAG_TYPE_INVERTED, section, &cur_section_end))
                                {
                                    free(section);
                                    return false;
                                }
                            }
                            input = cur_section_end;
                            free(section);
                        }
                        else
                        {
                            const char *cur_section_end = NULL;
                            if (!Render(out, start, input, hash_stack, delim_start, delim_start_len, delim_end, delim_end_len,
                                        tag.type != TAG_TYPE_INVERTED, section, &cur_section_end))
                            {
                                free(section);
                                return false;
                            }
                            free(section);
                            input = cur_section_end;
                        }
                        break;
                    }
                    break;
                }
            }
            continue;
        case TAG_TYPE_SECTION_END:
            if (!section)
            {
                char *varname = xstrndup(tag.content, tag.content_len);
                Log(LOG_LEVEL_WARNING, "Unknown section close in mustache template '%s'", varname);
                free(varname);
                return false;
            }
            else
            {
                SeqRemove(hash_stack, SeqLength(hash_stack) - 1);
                *section_end = input;
                return true;
            }
            break;

        default:
            assert(false);
            return false;
        }
    }

    assert(false);
}

bool MustacheRender(Buffer *out, const char *input, const JsonElement *hash)
{
    char delim_start[MUSTACHE_MAX_DELIM_SIZE] = "{{";
    size_t delim_start_len = strlen(delim_start);

    char delim_end[MUSTACHE_MAX_DELIM_SIZE] = "}}";
    size_t delim_end_len = strlen(delim_end);

    Seq *hash_stack = SeqNew(10, NULL);
    SeqAppend(hash_stack, (JsonElement*)hash);

    bool success = Render(out, input, input,
                         hash_stack,
                         delim_start, &delim_start_len,
                         delim_end, &delim_end_len,
                         false, NULL, NULL);

    SeqDestroy(hash_stack);

    return success;
}
