#!/bin/sh -e


if [ -z ${AWS_ACCESS_KEY_ID} ]
then
    export AWS_ACCESS_KEY_ID=`openssl rand -base64 20`
    export AWS_SECRET_ACCESS_KEY=`openssl rand -base64 40`

    radosgw-admin user create --uid ceph-test-librgw-file \
       --access-key $AWS_ACCESS_KEY_ID \
       --secret $AWS_SECRET_ACCESS_KEY \
       --display-name "librgw test user" \
       --email librgw@example.com || echo "librgw user exists"
fi

# nfsns is the main suite

# create herarchy, and then list it
echo "phase 1.1"
ceph_test_librgw_file_nfsns  --hier1 --dirs1 --create --rename --verbose

# the older librgw_file can consume the namespace
echo "phase 1.2"
ceph_test_librgw_file_nfsns --getattr --verbose

# and delete the hierarchy
echo "phase 1.3"
ceph_test_librgw_file_nfsns --hier1 --dirs1 --delete --verbose

# bulk create/delete buckets
echo "phase 2.1"
ceph_test_librgw_file_cd --create --multi --verbose
echo "phase 2.2"
ceph_test_librgw_file_cd --delete --multi --verbose

# write continuation test
echo "phase 3.1"
ceph_test_librgw_file_aw --create --delete --large --verify

# continued readdir
echo "phase 4.1"
ceph_test_librgw_file_marker --create --marker1 --marker2 --nobjs=100 --verbose

# advanced i/o--but skip readv/writev for now--split delete from
# create and stat ops to avoid fault in sysobject cache
echo "phase 5.1"
ceph_test_librgw_file_gp --get --stat --put --create
echo "phase 5.2"
ceph_test_librgw_file_gp --delete

exit 0
