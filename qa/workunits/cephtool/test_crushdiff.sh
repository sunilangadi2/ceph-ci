#!/usr/bin/env bash

set -ex

test $(ceph osd ls | wc -l) -ge 4

REP_POOL=
EC_POOL=
TEMPDIR=

setup() {
    local pool

    TEMPDIR=`mktemp -d`

    pool=test-crushdiff-rep-$$
    ceph osd pool create ${pool} 32
    REP_POOL=${pool}
    rados -p ${REP_POOL} bench 5 write --no-cleanup

    pool=test-crushdiff-ec-$$
    ceph osd pool create ${pool} 32 32 erasure
    EC_POOL=${pool}
    rados -p ${EC_POOL} bench 5 write --no-cleanup
}

cleanup() {
    set +e

    test -n "${EC_POOL}" &&
        ceph osd pool delete "${EC_POOL}" "${EC_POOL}" \
             --yes-i-really-really-mean-it
    test -n "${REP_POOL}" &&
        ceph osd pool delete "${REP_POOL}" "${REP_POOL}" \
             --yes-i-really-really-mean-it
    test -n "${TEMPDIR}" && rm -Rf ${TEMPDIR}
}

trap "cleanup" INT TERM EXIT

setup

# test without crushmap modification

crushdiff export ${TEMPDIR}/cm.txt --verbose
crushdiff compare ${TEMPDIR}/cm.txt --verbose
crushdiff import ${TEMPDIR}/cm.txt --verbose

# test using a compiled crushmap

crushdiff export ${TEMPDIR}/cm --compiled --verbose
crushdiff compare ${TEMPDIR}/cm --compiled --verbose
crushdiff import ${TEMPDIR}/cm --compiled --verbose

# test using "offline" osdmap and pg-dump

ceph osd getmap -o ${TEMPDIR}/osdmap
ceph pg dump --format json > ${TEMPDIR}/pg-dump

crushdiff export ${TEMPDIR}/cm.txt --osdmap ${TEMPDIR}/osdmap --verbose
crushdiff compare ${TEMPDIR}/cm.txt --osdmap ${TEMPDIR}/osdmap \
          --pg-dump ${TEMPDIR}/pg-dump --verbose | tee ${TEMPDIR}/compare.txt

# test the diff is zero when the crushmap is not modified

grep '^0/[0-9]* (0\.00%) pgs affected' ${TEMPDIR}/compare.txt
grep '^0/[0-9]* (0\.00%) objects affected' ${TEMPDIR}/compare.txt
grep '^0/[0-9]* (0\.00%) pg shards to move' ${TEMPDIR}/compare.txt
grep '^0/[0-9]* (0\.00%) pg object shards to move' ${TEMPDIR}/compare.txt
grep '^0\.00/.* (0\.00%) bytes to move' ${TEMPDIR}/compare.txt
crushdiff import ${TEMPDIR}/cm.txt --osdmap ${TEMPDIR}/osdmap --verbose

# test the diff is non-zero when the crushmap is modified

cat ${TEMPDIR}/cm.txt >&2

weight=$(awk '/item osd\.0 weight ([0-9.]+)/ {print $4 * 3}' ${TEMPDIR}/cm.txt)
test -n "${weight}"
sed -i -Ee 's/^(.*item osd\.0 weight )[0-9.]+/\1'${weight}'/' ${TEMPDIR}/cm.txt
crushdiff compare ${TEMPDIR}/cm.txt --osdmap ${TEMPDIR}/osdmap \
          --pg-dump ${TEMPDIR}/pg-dump --verbose | tee ${TEMPDIR}/compare.txt
grep '^[1-9][0-9]*/[0-9]* (.*%) pgs affected' ${TEMPDIR}/compare.txt
grep '^[1-9][0-9]*/[0-9]* (.*%) objects affected' ${TEMPDIR}/compare.txt
grep '^[1-9][0-9]*/[0-9]* (.*%) pg shards to move' ${TEMPDIR}/compare.txt
grep '^[1-9][0-9]*/[0-9]* (.*%) pg object shards to move' ${TEMPDIR}/compare.txt
grep '^.*/.* (.*%) bytes to move' ${TEMPDIR}/compare.txt
crushdiff import ${TEMPDIR}/cm.txt --osdmap ${TEMPDIR}/osdmap --verbose

echo OK
