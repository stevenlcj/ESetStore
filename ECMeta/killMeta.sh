#!/bin/bash
ps aux | grep ESetM | kill -9 $(gawk '{print $2}')
lsof -i:20001 | kill -9 $(gawk {'print $2'})

