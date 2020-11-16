from tasks.cephfs.cephfs_test_case import CephFSTestCase

log = logging.getLogger(__name__)

class TestFSTop(CephFSTestCase):
    # TODO: invoke fstop with --selftest and verify exit code
    def test_fstop(self):
        pass
