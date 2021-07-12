#!/bin/sh -x

set -e

echo "BUG: testing... kernel log failure" | sudo tee -a /var/log/kern.log
echo OK
