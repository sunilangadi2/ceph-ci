#!/bin/sh -x

set -e

sudo echo "Oops: testing... kernel log failure" >> /var/log/kern.log
echo OK
