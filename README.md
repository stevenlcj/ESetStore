This repository contains the related code for our work ESetStore, which is a prototype of erasure-coded storage systems with fast data recovery.

The repository contains five major components: ECClientLib, ECClientFS, ECMeta, ECServer, ECServerPPR.

ECClientLib: a client library for interact with the storage system;
ECClientFS: an encapsulation to write any file to and read any file from the storage system;
ECMeta: the code of our metadata service;
ECServer: the component for storing data to storage servers and retrieving data from storage servers;
ECServerPPR: the one with PPR algorithm integrated into our ESetStore.
