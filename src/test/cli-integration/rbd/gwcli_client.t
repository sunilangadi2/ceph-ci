Login to the target
===================
  $ IP=`cat /etc/ceph/ceph.conf |grep 'mon host' | awk -F: '{print $2}'`
  > iscsiadm -m discovery -t st -p $IP -l 2&> /dev/null
  $ ls /dev/disk/by-path/ |grep 'iscsi-iqn.2003-01.com.redhat.iscsi-gw:ceph-gw' |wc -l
  2

Make filesystem
===============
  $ device=`multipath -l | grep 'LIO-ORG,TCMU device' | awk '{print $1}'`
  > mkfs.xfs /dev/mapper/$device -f | grep 'meta-data=/dev/mapper/mpath' | awk '{print $2}'
  isize=512

  $ device=`multipath -l | grep 'LIO-ORG,TCMU device' | awk '{print $1}'`
  > blkid /dev/mapper/$device | awk '{print $3}'
  TYPE="xfs"

Write/Read test
===============
  $ device=`multipath -l | grep 'LIO-ORG,TCMU device' | awk '{print $1}'`
  > dd if=/dev/random of=/tmp/iscsi_tmpfile bs=1 count=1K 2&> /dev/null
  > dd if=/tmp/iscsi_tmpfile of=/dev/mapper/$device bs=1 count=1K 2&> /dev/null
  > dd if=/dev/mapper/$device of=/tmp/iscsi_tmpfile1 bs=1 count=1K 2&> /dev/null
  > diff /tmp/iscsi_tmpfile /tmp/iscsi_tmpfile1

Logout the targets
==================
  $ IP=`cat /etc/ceph/ceph.conf |grep 'mon host' | awk -F: '{print $2}'`
  > iscsiadm -m node -T iqn.2003-01.com.redhat.iscsi-gw:ceph-gw -p $IP:3260 -u | grep 'successful' | awk '{print $3}'
  successful.
  $ IP=`cat /etc/ceph/ceph.conf |grep 'mon host' | awk -F: '{print $6}'`
  > iscsiadm -m node -T iqn.2003-01.com.redhat.iscsi-gw:ceph-gw -p $IP:3260 -u | grep 'successful' | awk '{print $3}'
  successful.
