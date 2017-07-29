#include <test.h>
#include <nfs.c>
#include <nfs.h>
#include <item_lib.h>

static void test_MatchFSInFstab(void)
{
    AppendItem(&FSTABLIST, "fileserver1:/vol/vol10      /mnt/fileserver1/vol10     nfs     rw,intr,tcp,fg,rdirplus,noatime,_netdev", NULL);
    AppendItem(&FSTABLIST, "fileserver1:/vol/vol11      /mnt/fileserver1/vol11     nfs     rw,intr,tcp,fg,rdirplus,noatime,_netdev", NULL);
    AppendItem(&FSTABLIST, "UUID=4a147232-42f7-4e56-aa9e-744b09bce719 /               ext4    errors=remount-ro 0       1", NULL);
    AppendItem(&FSTABLIST, "UUID=b2cf5462-a10f-4d7d-b356-ecec5aea2103 none            swap    sw              0       0", NULL);
    AppendItem(&FSTABLIST, "none                     /proc/sys/fs/binfmt_misc             binfmt_misc defaults 0 0", NULL);
    AppendItem(&FSTABLIST, "fileserver2:/vol/vol10 	 /mnt/fileserver2/vol10 	 nfs 	 rw,intr,tcp,fg,noatime", NULL);
    AppendItem(&FSTABLIST, "fileserver2:/vol/vol11 	 /mnt/fileserver2/vol11 	 nfs 	 rw,intr,tcp,fg,noatime", NULL);

    assert_true(MatchFSInFstab("/mnt/fileserver1/vol10"));
    assert_true(MatchFSInFstab("/mnt/fileserver1/vol11"));
    assert_false(MatchFSInFstab("/mnt/fileserver1/vol1"));

    assert_true(MatchFSInFstab("/mnt/fileserver2/vol10"));
    assert_true(MatchFSInFstab("/mnt/fileserver2/vol11"));
    assert_false(MatchFSInFstab("/mnt/fileserver2/vol1"));
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] = {
        unit_test(test_MatchFSInFstab),
    };

    return run_tests(tests);
}


