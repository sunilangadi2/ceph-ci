#!/bin/sh -x

set -e

sudo echo "KASAN testing... kernel log failure" >> /var/log/kern.log
echo OK
