CC = gcc
LIBS = -pthread
CFLAGS = -Wall $(OPTFLAGS) $(LIBS)  -L/usr/local/lib

OBJS = ESetMeta

all: $(OBJS)

ESetMeta: ESetMeta.o ecUtils.o ECMetaEngine.o configProperties.o cfgParser.o rackMap.o ecNetHandle.o ESetPlacement.o BlockServer.o BlockServersManagement.o  metaServerBlockServerProtocol.o galois.o jerasure.o cauchy.o BlockManagement.o ECClientManagement.o ClientConnection.o ECClientCommand.o ECJob.o KeyValueSlab.o KeyValueEngine.o KeyValueWorker.o MessageBuffer.o hash.o BlockSlab.o ECClientCmdHandle.o ECRecoveryManagement.o ECBlockServerCmd.o
	$(CC) -o ESetMeta $(CFLAGS) ESetMeta.o ecUtils.o ECMetaEngine.o configProperties.o cfgParser.o ecNetHandle.o rackMap.o ESetPlacement.o BlockServer.o BlockServersManagement.o metaServerBlockServerProtocol.o galois.o jerasure.o cauchy.o BlockManagement.o ECClientManagement.o ClientConnection.o ECClientCommand.o ECJob.o KeyValueSlab.o KeyValueEngine.o KeyValueWorker.o MessageBuffer.o hash.o BlockSlab.o ECClientCmdHandle.o ECRecoveryManagement.o ECBlockServerCmd.o

ESetMeta.o: ECMetaEngine.h
ecUtils.o: ecUtils.h
ECMetaEngine.o: ECMetaEngine.h ecNetHandle.h ecCommon.h ESetPlacement.h
configProperties.o: configProperties.h
cfgParser.o: cfgParser.h
ecNetHandle.o: ecNetHandle.h
rackMap.o: rackMap.h ecUtils.h
ESetPlacement.o: ESetPlacement.h
BlockServer.o: BlockServer.h
BlockServersManagement.o: BlockServersManagement.h
metaServerBlockServerProtocol.o: metaServerBlockServerProtocol.h
galois.o: galois.h
jerasure.o: jerasure.h
cauchy.o: cauchy.h
BlockManagement.o: BlockManagement.h BlockSlab.h
BlockSlab.o: BlockSlab.h
MessageBuffer.o: MessageBuffer.h
ECClientManagement.o: ECClientManagement.h hash.h MessageBuffer.h
ClientConnection.o: ClientConnection.h MessageBuffer.h
ECClientCommand.o: ECClientCommand.h
ECJob.o: ECJob.h
hash.o: hash.h
KeyValueSlab.o: KeyValueSlab.h
KeyValueEngine.o: KeyValueEngine.h
KeyValueWorker.o: KeyValueWorker.h
ECClientCmdHandle.o: ECClientCmdHandle.h
ECRecoveryManagement.o: ECRecoveryManagement.h
ECBlockServerCmd.o: ECBlockServerCmd.h
.PHONY : clean
clean:
	-rm -rf ESetMeta *.o
