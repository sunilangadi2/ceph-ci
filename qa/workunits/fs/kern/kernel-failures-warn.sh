#!/bin/sh -x

set -e

echo "WARNING: testing... kernel log failure" | sudo tee -a /var/log/kern.log
echo OK
