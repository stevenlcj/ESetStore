This repository contains the related code for our work ESetStore, which is a prototype of erasure-coded storage systems with fast data recovery.

The repository contains five major components: ECClientLib, ECClientFS, ECMeta, ECServer, ECServerPPR.

ECClientLib: a client library for interact with the storage system;

ECClientFS: an encapsulation to write any file to and read any file from the storage system;

ECMeta: the code of our metadata service;

ECServer: the component for storing data to storage servers and retrieving data from storage servers;

ECServerPPR: the one with PPR algorithm integrated into our ESetStore.

Should you have any related question, feel free to contact me through liuchengjian@sztu.edu.cn
-------------------------------------------------------------------
Example of How to Build a storage system with 5 machines:
OS: Ubuntu 14.04
$HOME directory:/home/esetstore

IP Address and hostname:
192.168.0.100    metaServer

192.168.0.101    host01

192.168.0.102    host02

192.168.0.103    host03

192.168.0.104    client

#Step 1:

Deploy ECMeta on 192.168.0.100

Copy fold ECMeta into $HOME/ECMeta

$cd $HOME/ECMeta

type make to build the source code

The directory should have a program named ESetMeta after make

Now write the configuration file
$mkdir cfg
$vim cfg/MetaCfg

Type the following content into the file and save the file:

metaServer.clientPort = 20001

metaServer.chunkServerPort = 30001

metaServer.StripeK = 2

metaServer.StripeM = 1

metaServer.StripeW = 8

metaServer.StripePacketSizeInLong = 1

metaServer.streamingSize = 64

metaServer.overlappingFactor = 1

metaServer.blockSizeLimitsInMB = 64

metaServer.heartBeatIntervalInMS = 2000

metaServer.markServerFailedInterval = 5

metaServer.recoveryIntervalInSec = 5

metaServer.startRecoveryIntervalInSec = 3

metaServer.startRecoveryIntervalInSec = 3

$vim cfg/Servers

Type the following content into the file and save the file:

192.168.0.101 50001 0

192.168.0.102 50001 1

192.168.0.103 50001 2

--------------------------
Now run the ECMeta service with the following command:

$/home/esetstore/ECMeta/ESetMeta /home/esetstore/ECMeta/cfg/MetaCfg

#Step 2:

Deploy ECServer on 192.168.0.101, 192.168.0.102, and 192.168.0.103

Copy ECServer to the $HOME fold of 192.168.0.101, 192.168.0.102, and 192.168.0.103

Note that deploying ECServerPPR	is the same as the ECServer

Go to fold $HOME/ECServer

$make

After make, you should have a program named ESetServer

$mkdir $HOME/ECFile

The directory $HOME/ECFile is for storing data

$mkdir cfg

$vim $HOME/ECServer/cfg/ECServerCfg

Type the following content into the file and save the file, note that you should change the Chun:

chunkServer.MetaServerIP=192.168.0.100

chunkServer.MetaServerPort=30001

chunkServer.ChunkServerIP=192.168.0.101

chunkServer.ChunkServerPort=50001

chunkServer.blockPath=/home/esetstore/ECFile/

chunkServer.recoveryBufSizeInKB=256

chunkServer.recoveryBufNum=2
__________________________________
Now run the ECServer with the following command:

$/home/esetstore/ECServer/ESetServer /home/esetstore/ECServer/cfg/ECServerCfg

Now the storage server should connected with the metadata server, if it didn't work, please check if you open the TCP port 20001,30001 and 50001

#Step 3:

Deploy and Run Client on 192.168.0.104

Copy the Fold ECClientLib and ECClientFS to the directory of /home/esetstore on 192.168.0.104

Edit the .bashrc to add some environment variables

$vim /home/esetstore/.bashrc

Add the following content to the file:

export ESetSTORE_HOME=/home/esetstore/ECStorage

export PATH=$PATH:$ESetSTORE_HOME/bin

export LD_LIBRARY_PATH=${ESetSTORE_HOME}/lib:${LD_LIBRARY_PATH}

______________________________________________________________

$mkdir /home/esetstore/ECStorage

$mkdir /home/esetstore/ECStorage/cfg

$vim /home/esetstore/ECStorage/cfg/MetaServer

Type the following content into the file, save and exit:

MetaServer_IPADDR=192.168.0.100

MetaServer_Port=20001

_____________________________________________________________
Now go to fold /home/esetstore/ECClientLib	

type the following command

$make

$make install

If the command executed with no error, you should have libESetClient.so in /home/esetstore/ECStorage/lib

Then go to fold /home/esetstore/ECClientFS	

type the following command

$make

$make install

If the command executed with no error, you should have ECClientFS in /home/esetstore/ECStorage/bin

Now you can write file, e.g. file1,  to the storage system with the following command:

$ECClientFS -put file1

Then read the file from the storage system with the following command:

$ECClientFS -get file1



