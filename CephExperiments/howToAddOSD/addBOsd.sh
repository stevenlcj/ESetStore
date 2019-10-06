#!/bin/bash

if [ $# -lt 1 ]
then
  echo "Please specify the disks"
  exit
fi

ceph-deploy osd create --data /dev/sdb $1
