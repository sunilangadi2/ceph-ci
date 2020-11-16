import logging

from tasks.cephfs.cephfs_test_case import CephFSTestCase

log = logging.getLogger(__name__)

class TestFSTop(CephFSTestCase):
    def test_fstop_non_existent_cluster(self):
        proc = self.mount_a.run_shell(['cephfs-top', '--cluster=hpec', '--selftest'])
        self.assertTrue(proc.exitstatus != 0)

    def test_fstop_non_existent_cluster(self):
        proc = self.mount_a.run_shell(['cephfs-top', '--selftest'])
        self.assertTrue(proc.exitstatus == 0)
