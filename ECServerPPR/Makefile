CC = gcc
LIBS = -pthread -lm
CFLAGS = -Wall $(OPTFLAGS) $(LIBS)  -L/usr/local/lib

TARGET = ESetPPRServer
OBJS = ECServerMain.o ecNetHandle.o configProperties.o cfgParser.o BlockServerEngine.o ECMetaServerHandle.o ecUtils.o metaBlockServerCommand.o ClientHandle.o ClientBlockServerCmd.o MessageBuffer.o DiskIOHandler.o jerasure.o galois.o GCRSMatrix.o ECClientHandle.o ECClientResponse.o DiskJobQueue.o DiskJobWorker.o PPREngine.o PPRSlaveWorker.o PPRMasterWorker.o PPRCommand.o PPRMasterWorkerIOHandle.o PPRSlaveWorkerIOHandle.o

ALL = $(OBJS) $(TARGET)
all: $(ALL)

.SUFFIXES: .c .cu .o

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CFLAGS)

ECServerMain.o: ECMetaServerHandle.h
ecNetHandle.o: ecNetHandle.h ecCommon.h
ecUtils.o: ecUtils.h
configProperties.o: configProperties.h ecUtils.h
cfgParser.o: cfgParser.h
BlockServerEngine.o: BlockServerEngine.h
ECMetaServerHandle.o: ECMetaServerHandle.h
metaBlockServerCommand.o: metaBlockServerCommand.h
ClientHandle.o: ClientHandle.h ecCommon.h ClientBlockServerCmd.h MessageBuffer.h
ClientBlockServerCmd.o: ClientBlockServerCmd.h
DiskIOHandler.o: DiskIOHandler.h
MessageBuffer.o: MessageBuffer.h
jerasure.o: jerasure.h
galois.o: galois.h
GCRSMatrix.o: GCRSMatrix.h GCRSCommon.h
ECClientHandle.o: ECClientHandle.h ECClientResponse.h
ECClientResponse.o: ECClientResponse.h
DiskJobQueue.o: DiskJobQueue.h
DiskJobWorker.o: DiskJobWorker.h
PPREngine.o: PPREngine.h
PPRSlaveWorker.o: PPRSlaveWorker.h
PPRMasterWorker.o: PPRMasterWorker.h
PPRCommand.o: PPRCommand.h
PPRMasterWorkerIOHandle.o: PPRMasterWorkerIOHandle.h
PPRSlaveWorkerIOHandle.o: PPRSlaveWorkerIOHandle.h

.PHONY : clean
clean:
	-rm -rf $(TARGET) *.o
