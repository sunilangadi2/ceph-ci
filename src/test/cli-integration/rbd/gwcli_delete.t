Delete the host
===============
  $ gwcli iscsi-targets/iqn.2003-01.com.redhat.iscsi-gw:ceph-gw/hosts delete client_iqn=iqn.1994-05.com.redhat:client
  $ gwcli ls iscsi-targets/ | grep 'o- hosts' | awk -F'[' '{print $2}'
  Auth: ACL_ENABLED, Hosts: 0]

Delete the iscsi-targets disk
=============================
  $ gwcli iscsi-targets/iqn.2003-01.com.redhat.iscsi-gw:ceph-gw/disks/ delete disk=datapool/block0
  $ gwcli ls iscsi-targets/ | grep 'o- disks' | awk -F'[' '{print $2}'
  Disks: 0]

Delete the target IQN
=====================
  $ gwcli iscsi-targets/ delete target_iqn=iqn.2003-01.com.redhat.iscsi-gw:ceph-gw
  $ gwcli ls iscsi-targets/ | grep 'o- iscsi-targets' | awk -F'[' '{print $2}'
  DiscoveryAuth: None, Targets: 0]

Delete the disks
================
  $ gwcli disks/ delete image_id=datapool/block0
  $ gwcli ls disks/ | grep 'o- disks' | awk -F'[' '{print $2}'
  0.00Y, Disks: 0]
