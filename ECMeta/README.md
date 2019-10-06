This fold contains a directory named cfg that contains necessary file for our simple example.

You can run make and then run ./runMeta.sh to execute the program
To kill the program, run killMeta.sh

If you want to try another configuration. For example, set the value of I to 2 with six storage servers:
192.168.0.101 to 192.168.106

Edit the file MetaCfg
Change the :

metaServer.overlappingFactor = 1


To 

metaServer.overlappingFactor = 2


And then edit the file Servers
Change it to, the value 0, 1, 2 refers to the index of rack:
192.168.0.101 50001 0
192.168.0.102 50001 0
192.168.0.103 50001 1
192.168.0.104 50001 1
192.168.0.105 50001 2
192.168.0.106 50001 2

