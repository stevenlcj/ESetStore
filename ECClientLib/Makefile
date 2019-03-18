
# Compler and Linker
CXX := g++
CC := gcc

# Includes
INCLUDES = -I. 

Headers = ECClientEngine.h ECFileHandle.h
# Common flags
LIBS = -pthread -lm -lpthread
COMMONFLAGS += $(INCLUDES) -DPERASURE_CUDA_GM -DPERASURE_CUDA_SM
NVCCFLAGS += -ccbin g++ -m64 $(COMMONFLAGS)  -Xcompiler -fPIC -Xptxas -v --maxrregcount=32 -L/usr/local/cuda-8.0/nvvm/libdevice/libdevice.compute_50.10.bc
CXXFLAGS += -m64 $(COMMONFLAGS)
CFLAGS += -O3 $(COMMONFLAGS) -Wall $(OPTFLAGS) -L/usr/local/lib -fPIC -Wall -Wextra -O2 -g $(LIBS)

LDFLAGS = -shared

LINKLINE = $(CC) -o $(TARGET_LIB) $(OBJS)

TARGET_LIB = libESetClient.so
OBJS = galois.o jerasure.o GCRSMatrix.o ecUtils.o ECClientEngine.o ECNetHandle.o ECClientCommand.o ECFileHandle.o ECBlockWorker.o ECBlockWorkerIO.o ECStoreCoder.o MessageBuffer.o
ALL = $(OBJS) $(TARGET_LIB)
all: $(ALL)

.SUFFIXES: .c .cu .o
	
all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET_LIB) $(OBJS) $(CFLAGS)

galois.o: galois.h
jerasure.o: jerasure.h galois.h galois.o
GCRSMatrix.o: GCRSMatrix.h GCRSCommon.h jerasure.h
ecUtils.o: ecUtils.h
ECClientEngine.o: ECClientEngine.h ECClientCommon.h
ECNetHandle.o: ECNetHandle.h
ECClientCommand.o: ECClientCommand.h
ECFileHandle.o: ECFileHandle.h
ECBlockWorker.o: ECBlockWorker.h
ECBlockWorkerIO.o: ECBlockWorkerIO.h
ECStoreCoder.o: ECStoreCoder.h
MessageBuffer.o: MessageBuffer.h

.PHONY : clean
clean:
	-rm -rf $(TARGET_LIB) *.o

$(TARGET): $(OBJS)
	$(LINKLINE)

install:
	mkdir -p ${ESetSTORE_HOME}/lib
	mkdir -p ${ESetSTORE_HOME}/include
	cp ./$(TARGET_LIB) ${ESetSTORE_HOME}/lib
	cp ./${Headers} ${ESetSTORE_HOME}/include