#!/bin/sh -x

set -e

sudo echo "WARNING: testing... kernel log failure" >> /var/log/kern.log
echo OK
