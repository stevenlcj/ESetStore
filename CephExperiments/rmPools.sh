#!/bin/bash

poolName=$(sudo ceph osd lspools | gawk '{print $2}')

for pool in $poolName
do
  sudo ceph osd pool delete $pool $pool --yes-i-really-really-mean-it
done

