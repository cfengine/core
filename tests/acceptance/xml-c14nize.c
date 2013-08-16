#include <platform.h>

#include <libxml/parser.h>
#include <libxml/xpathInternals.h>
#include <libxml/c14n.h>

static bool xmlC14nizeFile(const char *filename)
{
    xmlDocPtr doc = xmlParseFile(filename);

    if (doc == NULL)
    {
        fprintf(stderr, "Unable to open %s for canonicalization\n", filename);
        return false;
    }

    xmlOutputBufferPtr out = xmlOutputBufferCreateFile(stdout, NULL);

    if (out == NULL)
    {
        fprintf(stderr, "Unable to set up writer for stdout\n");
        return false;
    }

    if (xmlC14NDocSaveTo(doc, NULL, XML_C14N_1_0, 0, true, out) < 0)
    {
        fprintf(stderr, "Unable to c14nize XML document\n");
        return false;
    }

    if (xmlOutputBufferClose(out) < 0)
    {
        fprintf(stderr, "Unable to close writer for stdout\n");
        return false;
    }

    xmlFreeDoc(doc);
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: xml-c14nize <XML file>\n");
        return 2;
    }

    return xmlC14nizeFile(argv[1]) ? 0 : 1;
}
