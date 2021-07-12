#!/bin/sh -x

set -e

echo "Oops: testing... kernel log failure" | sudo tee -a /home/ubuntu/cephtest/archive/syslog/kern.log
echo OK
