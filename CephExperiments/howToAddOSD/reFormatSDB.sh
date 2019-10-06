sudo dmsetup ls | sudo dmsetup remove $(gawk {'print $1'})
sudo parted -s /dev/sdb
sudo mkfs.xfs /dev/sdb -f
