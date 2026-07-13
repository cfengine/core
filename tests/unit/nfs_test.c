#include <test.h>
#include <nfs.c>
#include <nfs.h>
#include <item_lib.h>

static void test_MatchFSInFstab(void)
{
    AppendItem(&FSTABLIST, "fileserver1:/vol/vol10      /mnt/fileserver1/vol10     nfs     rw,intr,tcp,fg,rdirplus,noatime,_netdev", NULL);
    AppendItem(&FSTABLIST, "fileserver1:/vol/vol11      /mnt/fileserver1/vol11     nfs     rw,intr,tcp,fg,rdirplus,noatime,_netdev", NULL);
    AppendItem(&FSTABLIST, "#fileserver1:/vol/vol12     /mnt/fileserver1/vol12     nfs     rw,intr,tcp,fg,rdirplus,noatime,_netdev", NULL);
    AppendItem(&FSTABLIST, "UUID=4a147232-42f7-4e56-aa9e-744b09bce719 /               ext4    errors=remount-ro 0       1", NULL);
    AppendItem(&FSTABLIST, "UUID=b2cf5462-a10f-4d7d-b356-ecec5aea2103 none            swap    sw              0       0", NULL);
    AppendItem(&FSTABLIST, "none                     /proc/sys/fs/binfmt_misc             binfmt_misc defaults 0 0", NULL);
    AppendItem(&FSTABLIST, "fileserver2:/vol/vol10 	 /mnt/fileserver2/vol10 	 nfs 	 rw,intr,tcp,fg,noatime", NULL);
    AppendItem(&FSTABLIST, "fileserver2:/vol/vol11 	 /mnt/fileserver2/vol11 	 nfs 	 rw,intr,tcp,fg,noatime", NULL);
    AppendItem(&FSTABLIST, "#fileserver2:/vol/vol12 	 /mnt/fileserver2/vol12 	 nfs 	 rw,intr,tcp,fg,noatime", NULL);
    AppendItem(&FSTABLIST, "fileserver3:/vol/vol10 	 /mnt/fileserver3/vol10 	 nfs 	 rw,intr,tcp,fg #,noatime", NULL);
    AppendItem(&FSTABLIST, "fileserver3:/vol/vol11 	 /mnt/fileserver3/vol11 	 nfs 	 rw,intr,tcp,fg ,noatime # do we want noatime?", NULL);

    assert_true(MatchFSInFstab("/mnt/fileserver1/vol10"));
    assert_true(MatchFSInFstab("/mnt/fileserver1/vol11"));
    assert_false(MatchFSInFstab("/mnt/fileserver1/vol1"));

    assert_true(MatchFSInFstab("/mnt/fileserver2/vol10"));
    assert_true(MatchFSInFstab("/mnt/fileserver2/vol11"));
    assert_false(MatchFSInFstab("/mnt/fileserver2/vol1"));

    assert_false(MatchFSInFstab("/mnt/fileserver1/vol12"));
    assert_false(MatchFSInFstab("/mnt/fileserver2/vol12"));

    assert_true(MatchFSInFstab("/mnt/fileserver3/vol10"));
    assert_true(MatchFSInFstab("/mnt/fileserver3/vol11"));
    assert_false(MatchFSInFstab("/mnt/fileserver3/vol1"));
}

static void test_OptionsSubsetMatches(void)
{
    /* Empty/NULL promise is always satisfied. */
    assert_true(OptionsSubsetMatches(NULL, "rw,noatime"));
    assert_true(OptionsSubsetMatches("", "rw,noatime"));

    /* Subset: all promised present, kernel-added options ignored. */
    assert_true(OptionsSubsetMatches("rw,noatime",
        "rw,noatime,vers=4.2,rsize=524288,wsize=524288,hard,proto=tcp,addr=10.0.0.1"));

    /* Order-insensitive. */
    assert_true(OptionsSubsetMatches("noatime,rw", "rw,noatime,vers=4.2"));

    /* A promised option that is simply absent -> mismatch. */
    assert_false(OptionsSubsetMatches("rw,noatime,acl", "rw,noatime,vers=4.2"));

    /* Inverse pairs contradict. */
    assert_false(OptionsSubsetMatches("noatime", "rw,relatime,vers=4.2"));
    assert_false(OptionsSubsetMatches("ro", "rw,relatime"));
    assert_false(OptionsSubsetMatches("rw", "ro,relatime"));
    assert_false(OptionsSubsetMatches("hard", "rw,soft"));
    assert_false(OptionsSubsetMatches("sync", "rw,async"));

    /* Generic "no<opt>" vs "<opt>" contradiction. */
    assert_false(OptionsSubsetMatches("nodev", "rw,dev"));
    assert_false(OptionsSubsetMatches("atime", "rw,noatime"));

    /* Protocol aliases, both directions. */
    assert_true(OptionsSubsetMatches("tcp", "rw,proto=tcp,vers=4.2"));
    assert_true(OptionsSubsetMatches("proto=tcp", "rw,tcp"));
    assert_true(OptionsSubsetMatches("udp", "rw,proto=udp"));

    /* "defaults" is never echoed by the kernel; it holds iff none of the
     * negatives that would violate it (ro/nosuid/nodev/noexec/sync) are
     * present.  atime/relatime and kernel-added options are irrelevant to it. */
    assert_true(OptionsSubsetMatches("defaults", "rw,relatime,vers=4.2,hard"));
    assert_true(OptionsSubsetMatches("defaults,noatime", "rw,noatime,vers=4.2"));
    assert_true(OptionsSubsetMatches("defaults", "rw,noatime,relatime,vers=4.2"));
    assert_false(OptionsSubsetMatches("defaults", "ro,relatime,vers=4.2"));
    assert_false(OptionsSubsetMatches("defaults", "rw,nosuid,vers=4.2"));
    assert_false(OptionsSubsetMatches("defaults", "rw,noexec"));
    assert_false(OptionsSubsetMatches("defaults", "rw,sync"));

    /* Last wins: a later option overrides an earlier conflicting one, exactly
     * as `mount -o` applies the list (defaults,ro is read-only; ro,rw is rw). */
    assert_true(OptionsSubsetMatches("defaults,ro", "ro,relatime,vers=4.2"));    /* ro overrides defaults' rw */
    assert_false(OptionsSubsetMatches("defaults,ro", "rw,relatime,vers=4.2"));   /* wants ro, mount is rw */
    assert_true(OptionsSubsetMatches("ro,rw", "rw,relatime"));                   /* rw wins */
    assert_false(OptionsSubsetMatches("ro,rw", "ro,relatime"));                  /* wants rw, mount is ro */
    assert_true(OptionsSubsetMatches("rw,ro", "ro,relatime"));                   /* ro wins */
    assert_true(OptionsSubsetMatches("defaults,nosuid", "rw,nosuid,vers=4.2"));  /* nosuid overrides defaults' suid */
}

static void test_RemountOptionString(void)
{
    char *s;

    /* "defaults" expands to its checkable positives so an in-place remount
     * restores a mount that drifted to ro/nosuid/etc (util-linux does not honor
     * the options implied by "defaults" on a remount). */
    s = RemountOptionString("defaults");
    assert_true(strcmp(s, "rw,suid,dev,exec,async") == 0); free(s);
    s = RemountOptionString("defaults,noatime");
    assert_true(strcmp(s, "rw,suid,dev,exec,async,noatime") == 0); free(s);
    /* Everything else is passed through unchanged and order-preserved. */
    s = RemountOptionString("rw,noatime");       assert_true(strcmp(s, "rw,noatime") == 0); free(s);
    s = RemountOptionString("ro");               assert_true(strcmp(s, "ro") == 0); free(s);
    assert_true(RemountOptionString("") == NULL);
    assert_true(RemountOptionString(NULL) == NULL);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] = {
        unit_test(test_MatchFSInFstab),
        unit_test(test_OptionsSubsetMatches),
        unit_test(test_RemountOptionString),
    };

    return run_tests(tests);
}


