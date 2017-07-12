/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/


#include <platform.h>

#include <openssl/buffer.h>                                 /* BUF_MEM */
#include <openssl/bio.h>                                    /* BIO_* */
#include <openssl/evp.h>                                    /* BIO_f_base64 */

#include <alloc.h>


char *StringEncodeBase64(const char *str, size_t len)
{
    assert(str);
    if (!str)
    {
        return NULL;
    }

    if (len == 0)
    {
        return xcalloc(1, sizeof(char));
    }

    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bio = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bio);
    BIO_write(b64, str, len);
    if (!BIO_flush(b64))
    {
        assert(false && "Unable to encode string to base64" && str);
        return NULL;
    }

    BUF_MEM *buffer = NULL;
    BIO_get_mem_ptr(b64, &buffer);
    char *out = xcalloc(1, buffer->length);
    memcpy(out, buffer->data, buffer->length - 1);
    out[buffer->length - 1] = '\0';

    BIO_free_all(b64);

    return out;
}
