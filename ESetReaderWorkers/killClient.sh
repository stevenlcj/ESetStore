ps aux | grep ECClientFS | kill -9 $(gawk {'print $2'})
