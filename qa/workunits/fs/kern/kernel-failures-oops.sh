#!/bin/sh -x

set -e

echo "Oops: testing... kernel log failure" | sudo tee /var/log/kern.log
echo OK
