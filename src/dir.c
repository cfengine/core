#include "cf3.defs.h"
#include "cf3.extern.h"

static size_t GetNameMax(DIR *dirp);
static size_t GetDirentBufferSize(size_t path_len);
static void CloseDirRemote(CFDIR *dir);

void CloseDirLocal(CFDIR *dir);
const struct dirent *ReadDirLocal(CFDIR *dir);

/*********************************************************************/

CFDIR *OpenDirForPromise(const char *dirname, struct Attributes attr, struct Promise *pp)
{
if (attr.copy.servers == NULL || strcmp(attr.copy.servers->item,"localhost") == 0)
   {
   return OpenDirLocal(dirname);
   }
else
   {
   /* -> client_code.c to talk to server */
   return OpenDirRemote(dirname, attr, pp);
   }
}

/*********************************************************************/

#ifndef MINGW
CFDIR *OpenDirLocal(const char *dirname)
{
CFDIR *ret;
if ((ret = calloc(1, sizeof(CFDIR))) == NULL)
   {
   FatalError("Unable to allocate memory for CFDIR");
   }
DIR *dirh;

ret->dirh = dirh  = opendir(dirname);
if (dirh == NULL)
   {
   free(ret);
   return NULL;
   }

size_t dirent_buf_size = GetDirentBufferSize(GetNameMax(dirh));
if (dirent_buf_size == (size_t)-1)
   {
   FatalError("Unable to determine directory entry buffer size for directory %s", dirname);
   }

if ((ret->entrybuf = calloc(1, dirent_buf_size)) == NULL)
   {
   FatalError("Unable to allocate memory for directory entry buffer for directory %s", dirname);
   }

return ret;
}
#endif

/*********************************************************************/

static const struct dirent *ReadDirRemote(CFDIR *dir)
{
const char *ret = NULL;

if (dir->listpos != NULL)
   {
   ret = dir->listpos->name;
   dir->listpos = dir->listpos->next;
   }

return (struct dirent*)ret;
}

/*********************************************************************/

#ifndef MINGW
/*
 * Returns NULL on EOF or error.
 *
 * Sets errno to 0 for EOF and non-0 for error.
 */
const struct dirent *ReadDirLocal(CFDIR *dir)
{
int err;
struct dirent *ret;

errno = 0;
err = readdir_r((DIR *)dir->dirh, dir->entrybuf, &ret);

if (err != 0)
   {
   errno = err;
   return NULL;
   }

if (ret == NULL)
   {
   return NULL;
   }

return ret;
}
#endif

/*********************************************************************/

const struct dirent *ReadDir(CFDIR *dir)
{
if (dir->list)
   {
   return ReadDirRemote(dir);
   }
else if (dir->dirh)
   {
   return ReadDirLocal(dir);
   }
else
   {
   FatalError("CFDIR passed has no list nor directory handle open");
   }
}

/*********************************************************************/

void CloseDir(CFDIR *dir)
{
if (dir->dirh)
   {
   CloseDirLocal(dir);
   }
else
   {
   CloseDirRemote(dir);
   }
}

/*********************************************************************/

static void CloseDirRemote(CFDIR *dir)
{
if (dir->list)
   {
   DeleteItemList(dir->list);
   }
free(dir);
}

/*********************************************************************/

#ifndef MINGW
void CloseDirLocal(CFDIR *dir)
{
closedir((DIR *)dir->dirh);
free(dir->entrybuf);
free(dir);
}
#endif

/*********************************************************************/

#ifndef MINGW
/*
 * Taken from http://womble.decadent.org.uk/readdir_r-advisory.html
 *
 * Issued by Ben Hutchings <ben@decadent.org.uk>, 2005-11-02.
 *
 * Licence
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following condition:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Calculate the required buffer size (in bytes) for directory entries read from
 * the given directory handle.  Return -1 if this this cannot be done.
 *
 * This code does not trust values of NAME_MAX that are less than 255, since
 * some systems (including at least HP-UX) incorrectly define it to be a smaller
 * value.
 *
 * If you use autoconf, include fpathconf and dirfd in your AC_CHECK_FUNCS list.
 * Otherwise use some other method to detect and use them where available.
 */

#if defined(HAVE_FPATHCONF) && defined(_PC_NAME_MAX)

static size_t GetNameMax(DIR *dirp)
{
long name_max = fpathconf(dirfd(dirp), _PC_NAME_MAX);
if (name_max != -1)
   {
   return name_max;
   }

#if defined(NAME_MAX)
return (NAME_MAX > 255) ? NAME_MAX : 255;
#else
return (size_t)(-1);
#endif
}

#else /* FPATHCONF && _PC_NAME_MAX */

# if defined(NAME_MAX)
static size_t GetNameMax(DIR *dirp)
{
return (NAME_MAX > 255) ? NAME_MAX : 255;
}
# else
#  error "buffer size for readdir_r cannot be determined"
# endif

#endif /* FPATHCONF && _PC_NAME_MAX */

/*********************************************************************/

/*
 * Returns size of memory enough to hold path name_len bytes long.
 */
static size_t GetDirentBufferSize(size_t name_len)
{
size_t name_end = (size_t)offsetof(struct dirent, d_name) + name_len + 1;
return (name_end > sizeof(struct dirent) ? name_end : sizeof(struct dirent));
}

/*********************************************************************/

struct dirent *AllocateDirentForFilename(const char *filename)
{
struct dirent *entry = calloc(1, GetDirentBufferSize(strlen(filename)));
strcpy(entry->d_name, filename);
return entry;
}
#endif
