#!/bin/bash
ps aux | grep Eset | kill -9 $(gawk {'print $2'})
rm -rf /home/esetstore/ECFile/*
