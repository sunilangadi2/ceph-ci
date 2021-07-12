#!/bin/sh -x

set -e

echo "INFO: task .+ blocked for more than .+ seconds" | sudo tee -a /var/log/kern.log
echo OK
