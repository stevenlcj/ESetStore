sudo ceph tell 'osd.*' injectargs '--osd-max-backfills 8'
sudo ceph tell 'osd.*' injectargs '--osd-recovery-max-active 8'
