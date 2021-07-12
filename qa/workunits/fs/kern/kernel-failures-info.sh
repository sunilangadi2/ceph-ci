#!/bin/sh -x

set -e

echo "INFO: task .+ blocked for more than .+ seconds" | sudo tee -a /home/ubuntu/cephtest/archive/syslog/kern.log
echo OK
