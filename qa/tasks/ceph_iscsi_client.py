"""
Set up ceph-iscsi client
"""
import logging
import contextlib
from teuthology.orchestra import run
from textwrap import dedent

log = logging.getLogger(__name__)

class IscsiClientSetup(object):
    def __init__(self, ctx, config):
        self.ctx = ctx
        self.config = config
        self.client_iqn = "iqn.1994-05.com.redhat:client"

    def ceph_iscsi_client(self):

        """Spawned task that setup ceph iSCSI client"""

        for role in self.config['clients']:

            (remote,) = (self.ctx.cluster.only(role).remotes.keys())

            conf = dedent(f'''
            InitiatorName={self.client_iqn}
            ''')
            path = "/etc/iscsi/initiatorname.iscsi"
            remote.sudo_write_file(path, conf, mkdir=True)

            # the restart is needed after the above change is applied
            remote.run(args=['sudo', 'systemctl', 'restart', 'iscsid'])

            remote.run(args=['sudo', 'modprobe', 'dm_multipath'])
            remote.run(args=['sudo', 'mpathconf', '--enable'])
            conf = dedent('''
            devices {
                    device {
                            vendor                 "LIO-ORG"
                            product                "TCMU device"
                            hardware_handler       "1 alua"
                            path_grouping_policy   "failover"
                            path_selector          "queue-length 0"
                            failback               60
                            path_checker           tur
                            prio                   alua
                            prio_args              exclusive_pref_bit
                            fast_io_fail_tmo       25
                            no_path_retry          queue
                    }
            }
            ''')
            path = "/etc/multipath.conf"
            remote.sudo_write_file(path, conf, append=True)
            remote.run(args=['sudo', 'systemctl', 'start', 'multipathd'])


@contextlib.contextmanager
def task(ctx, config):
    """
    Specify which clients to run on as a list::

      tasks:
        ceph_iscsi_client:
          clients: [client.1]
    """
    log.info('Setting up ceph iscsi client...')
    iscsi = IscsiClientSetup(ctx, config)
    iscsi.ceph_iscsi_client()

    yield
