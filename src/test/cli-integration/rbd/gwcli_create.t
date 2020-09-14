Create a datapool/block0 disk
=============================
  $ gwcli disks/ create pool=datapool image=block0 size=300M wwn=36001405da17b74481464e9fa968746d3
  $ gwcli ls disks/ | grep 'o- disks' | awk -F'[' '{print $2}'
  300M, Disks: 1]
  $ gwcli ls disks/ | grep 'o- disks' | awk -F'[' '{print $2}'
  300M, Disks: 1]
  $ gwcli ls disks/ | grep 'o- datapool' | awk -F'[' '{print $2}'
  datapool (300M)]
  $ gwcli ls disks/ | grep 'o- block0' | awk -F'[' '{print $2}'
  datapool/block0 (Online, 300M)]

Create the target IQN
=====================
  $ gwcli iscsi-targets/ create target_iqn=iqn.2003-01.com.redhat.iscsi-gw:ceph-gw
  $ gwcli ls iscsi-targets/ | grep 'o- iscsi-targets' | awk -F'[' '{print $2}'
  DiscoveryAuth: None, Targets: 1]
  $ gwcli ls iscsi-targets/ | grep 'o- iqn.2003-01.com.redhat.iscsi-gw:ceph-gw' | awk -F'[' '{print $2}'
  Auth: None, Gateways: 0]
  $ gwcli ls iscsi-targets/ | grep 'o- disks' | awk -F'[' '{print $2}'
  Disks: 0]
  $ gwcli ls iscsi-targets/ | grep 'o- gateways' | awk -F'[' '{print $2}'
  Up: 0/0, Portals: 0]
  $ gwcli ls iscsi-targets/ | grep 'o- host-groups' | awk -F'[' '{print $2}'
  Groups : 0]
  $ gwcli ls iscsi-targets/ | grep 'o- hosts' | awk -F'[' '{print $2}'
  Auth: ACL_ENABLED, Hosts: 0]

Create the first gateway
========================
  $ HOST=`hostname`
  > IP=`hostname -i | awk '{print $1}'`
  > gwcli iscsi-targets/iqn.2003-01.com.redhat.iscsi-gw:ceph-gw/gateways create ip_addresses=$IP gateway_name=$HOST
  $ gwcli ls iscsi-targets/ | grep 'o- gateways' | awk -F'[' '{print $2}'
  Up: 1/1, Portals: 1]

Create the second gateway
========================
  $ IP=`cat /etc/ceph/ceph.conf |grep 'mon host' | awk -F: '{print $2}'`
  > HOST=`python3 -c "import socket; print(socket.getfqdn('$IP'))"`
  > if [ "$IP" != `hostname -i | awk '{print $1}'` ]; then
  >   gwcli iscsi-targets/iqn.2003-01.com.redhat.iscsi-gw:ceph-gw/gateways create ip_addresses=$IP gateway_name=$HOST
  > fi
  $ IP=`cat /etc/ceph/ceph.conf |grep 'mon host' | awk -F: '{print $6}'`
  > HOST=`python3 -c "import socket; print(socket.getfqdn('$IP'))"`
  > if [ "$IP" != `hostname -i | awk '{print $1}'` ]; then
  >   gwcli iscsi-targets/iqn.2003-01.com.redhat.iscsi-gw:ceph-gw/gateways create ip_addresses=$IP gateway_name=$HOST
  > fi
  $ gwcli ls iscsi-targets/ | grep 'o- gateways' | awk -F'[' '{print $2}'
  Up: 2/2, Portals: 2]

Attach the disk
===============
  $ gwcli iscsi-targets/iqn.2003-01.com.redhat.iscsi-gw:ceph-gw/disks/ add disk=datapool/block0
  $ gwcli ls iscsi-targets/ | grep 'o- disks' | awk -F'[' '{print $2}'
  Disks: 1]

Create a host
=============
  $ gwcli iscsi-targets/iqn.2003-01.com.redhat.iscsi-gw:ceph-gw/hosts create client_iqn=iqn.1994-05.com.redhat:client
  $ gwcli ls iscsi-targets/ | grep 'o- hosts' | awk -F'[' '{print $2}'
  Auth: ACL_ENABLED, Hosts: 1]
  $ gwcli ls iscsi-targets/ | grep 'o- iqn.1994-05.com.redhat:client' | awk -F'[' '{print $2}'
  Auth: None, Disks: 0(0.00Y)]

Map the LUN
===========
  $ gwcli iscsi-targets/iqn.2003-01.com.redhat.iscsi-gw:ceph-gw/hosts/iqn.1994-05.com.redhat:client disk disk=datapool/block0
  $ gwcli ls iscsi-targets/ | grep 'o- hosts' | awk -F'[' '{print $2}'
  Auth: ACL_ENABLED, Hosts: 1]
  $ gwcli ls iscsi-targets/ | grep 'o- iqn.1994-05.com.redhat:client' | awk -F'[' '{print $2}'
  Auth: None, Disks: 1(300M)]
