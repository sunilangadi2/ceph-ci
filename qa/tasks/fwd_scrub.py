"""
Thrash mds by simulating failures
"""
import logging
import contextlib

from gevent import sleep, wait
from gevent.greenlet import Greenlet
from gevent.event import Event
from teuthology import misc as teuthology
from teuthology import contextutil

from tasks import ceph_manager
from tasks.cephfs.filesystem import MDSCluster, Filesystem
from tasks.thrasher import Thrasher

log = logging.getLogger(__name__)


class MDSRankScrubber(Thrasher, Greenlet):
    def __init__(self, fs, mds_rank):
        super(MDSRankScrubber, self).__init__()
        self.fs = fs
        self.mds_rank = mds_rank

    def _run(self):
        try:
            self.do_scrub()
        except Exception as e:
            self.set_thrasher_exception(e)
            self.logger.exception("exception:")
            # allow successful completion so gevent doesn't see an exception...

    def _wait_until_scrub_complete(self, path="/", recursive=True):
        out_json = self.fs.rank_tell(["scrub", "start", path] +
                                     ["recursive"] if recursive else [],
                                     rank=self.mds_rank)
        with contextutil.safe_while(sleep=30, tries=30) as proceed:
            while proceed():
                out_json = self.fs.rank_tell(["scrub", "status"],
                                             rank=self.mds_rank)
                if out_json['status'] == "no active scrubs running":
                    break

    def do_scrub(self):
        self._wait_until_scrub_complete()


class ForwardScrubber(Thrasher, Greenlet):
    """
    ForwardScrubber::

    The ForwardScrubber does forward scrubbing of file-systems during execution
    of other tasks (workunits, etc).

    """
    def __init__(self, fs):
        super(ForwardScrubber, self).__init__()

        self.logger = log.getChild('fs.[{f}]'.format(f=fs.name))
        self.fs = fs
        self.name = 'thrasher.fs.[{f}]'.format(f=fs.name)
        self.stopping = Event()

    def _run(self):
        try:
            self.do_scrub()
        except Exception as e:
            self.set_thrasher_exception(e)
            self.logger.exception("exception:")
            # allow successful completion so gevent doesn't see an exception...

    def log(self, x):
        """Write data to the logger assigned to MDSThrasher"""
        self.logger.info(x)

    def stop(self):
        self.stopping.set()

    def do_scrub(self):
        """
        Perform the file-system scrubbing
        """
        self.log(f'starting do_scrub for fs: {self.fs.name}')

        while not self.stopping.is_set():
            scrubbers = []
            ranks = self.fs.get_all_mds_rank("up:active")

            for r in ranks:
                scrubber = MDSRankScrubber(self.fs, r)
                scrubbers.append(scrubber)
                scrubber.start()

            # wait for all scrubbers to complete
            completed_scrubbers = wait(scrubbers, count=len(scrubbers))

            for cs in completed_scrubbers:
                cs.join()
                if cs.exception is not None:
                    raise RuntimeError('error during scrub thrashing')

            scrubbers.clear()
            sleep(1)


@contextlib.contextmanager
def task(ctx, config):
    """
    Stress test the mds by running scrub iterations while another task/workunit
    is running.
    """

    mds_cluster = MDSCluster(ctx)

    if config is None:
        config = {}
    assert isinstance(config, dict), \
        'fwd_scrub task only accepts a dict for configuration'
    mdslist = list(teuthology.all_roles_of_type(ctx.cluster, 'mds'))
    assert len(mdslist) > 0, \
        'fwd_scrub task requires at least 1 metadata server'

    (first,) = ctx.cluster.only(f'mds.{mdslist[0]}').remotes.keys()
    manager = ceph_manager.CephManager(
        first, ctx=ctx, logger=log.getChild('ceph_manager'),
    )

    # make sure everyone is in active, standby, or standby-replay
    log.info('Wait for all MDSs to reach steady state...')
    status = mds_cluster.status()
    while True:
        steady = True
        for info in status.get_all():
            state = info['state']
            if state not in ('up:active', 'up:standby', 'up:standby-replay'):
                steady = False
                break
        if steady:
            break
        sleep(2)
        status = mds_cluster.status()

    log.info('Ready to start scrub thrashing')

    manager.wait_for_clean()
    assert manager.is_clean()

    if 'cluster' not in config:
        config['cluster'] = 'ceph'

    for fs in status.get_filesystems():
        fwd_scrubber = ForwardScrubber(Filesystem(ctx, fs['id']))
        fwd_scrubber.start()
        ctx.ceph[config['cluster']].thrashers.append(fwd_scrubber)

    try:
        log.debug('Yielding')
        yield
    finally:
        log.info('joining ForwardScrubbers')
        for fwd_scrubber in ctx.ceph[config['cluster']].thrashers:
            fwd_scrubber.stop()
            if fwd_scrubber.exception is not None:
                raise RuntimeError('error during scrub thrashing')
            fwd_scrubber.join()
        log.info('done joining')
