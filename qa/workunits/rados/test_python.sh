#!/bin/sh -ex

ceph osd pool create rbd
${PYTHON:-python3} -m nose -v $(dirname $0)/../../../src/test/pybind/test_rados.py:TestWatchNotify "$@"
exit 0
