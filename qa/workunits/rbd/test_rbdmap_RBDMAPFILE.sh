#!/bin/sh
#
# Regression test for http://tracker.ceph.com/issues/14984
#
# When the bug is present, starting the rbdmap service causes
# a bogus log message to be emitted to the log because the RBDMAPFILE
# environment variable is not set.
#
# When the bug is not present, starting the rbdmap service will emit
# no log messages, because /etc/ceph/rbdmap does not contain any lines
# that require processing.
#
set -ex

echo "TEST: save timestamp for use later with journalctl --since"
TIMESTAMP=$(date +%Y-%m-%d\ %H:%M:%S)

echo "TEST: assert that rbdmap-generator has not logged anything since boot"
journalctl -b 0 -t rbdmap-generator | grep 'rbdmap-generator\[[[:digit:]]' && exit 1

echo "TEST: systemd reload to trigger generation"
sudo systemctl daemon-reload

echo "TEST: assert that rbdmap-generator has not logged anything since TIMESTAMP"
journalctl --since "$TIMESTAMP" -t rbdmap-generator | grep 'rbdmap-generator\[[[:digit:]]' && exit 1
journalctl --since "$TIMESTAMP" -t systemd | grep 'rbdmap-generator failed' && exit 1

exit 0
