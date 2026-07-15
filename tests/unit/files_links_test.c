/*
  Copyright 2026 Northern.tech AS

  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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

#include <test.h>

#include <cf3.defs.h>
#include <files_links.h>                                       /* ExpandLinks */

/* A single path component longer than the internal node buffer
 * (CF_MAXLINKSIZE) overflowed the stack in ExpandLinks(): the parse into
 * 'node' ran before any lstat(), so a symlink target with a long slash-free
 * component smashed the stack. It must bail out instead. */
static void test_expand_links_long_component(void)
{
    char dest[CF_BUFSIZE];

    char from[1024];
    from[0] = '/';
    memset(from + 1, 'a', sizeof(from) - 2);
    from[sizeof(from) - 1] = '\0';

    bool ret = ExpandLinks(dest, from, 0, CF_MAXLINKLEVEL);
    assert_false(ret);
}

int main()
{
    PRINT_TEST_BANNER();

    const UnitTest tests[] = {
        unit_test(test_expand_links_long_component),
    };

    return run_tests(tests);
}
