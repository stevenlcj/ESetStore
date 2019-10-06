#!/bin/bash
cmdStr=$(lsof -i:50001)
pidStr=$(echo "$cmdStr" | grep -oP '(?<=ESetServe ).*(?= t716)')
rm -rf /home/esetstore/ECFile/*
echo $pidStr
kill -9 $pidStr
