#!/bin/sh -x

set -e

sudo echo "INFO: task .+ blocked for more than .+ seconds" >> /var/log/kern.log
echo OK
