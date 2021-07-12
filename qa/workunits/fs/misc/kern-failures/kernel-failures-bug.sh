#!/bin/sh -x

set -e

sudo echo "BUG: testing... kernel log failure" >> /var/log/kern.log
echo OK
