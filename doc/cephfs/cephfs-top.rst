==================
CephFS Top Utility
==================

CephFS provides `top(1)` like utility to display various Ceph Filesystem metrics
in realtime. `cephfs-top` is a curses based python script which makes use of `stats`
plugin in Ceph Manager to fetch (and display) metrics.

Manager Plugin
--------------

Ceph Filesystem clients periodically forward various metrics to Ceph Metadata Servers (MDS)
which in turn get forwarded to Ceph Manager by MDS rank zero. Each active MDS forward its
respective set of metrics to MDS rank zero. Metrics are aggergated and forwarded to Ceph
Manager.

Metrics can be divided into two categories - global and per-mds. Global metrics represent
set of metrics for the filesystem as a whole (e.g., client read latency) whereas per-mds
metrics are for a particular MDS rank (e.g., number of subtrees handled by an MDS).

.. note:: Currently, only global metrics are tracked.


`cephfs-top`
------------

`cephfs-top` utility relies on `stats` plugin to fetch performance metrics and display in
`top(1)` like format. `cephfs-top` is available as part of `cephfs-top` package.

By default, `cephfs-top` uses `client.fstop` user to connect to a Ceph cluster::

  $ ceph auth get-or-create client.fstop mon 'allow r' mds 'allow r' osd 'allow r' mgr 'allow r'
  $ cephfs-top

To use a non-default user (other than `client.xfstop`) use::

  $ cephfs-top --client <user>

By default, `cephfs-top` connects to cluster name `ceph`. To use a non-default cluster name::

  $ cephfs-top --cluster <cluster>
