CC = gcc

libECClient = ESetClient
#test ! -f ${ECSTORE_HOME}/lib/libECClientGCRS.so  && libECClient = ECClient

INCLUDES = -I. -I${ESetSTORE_HOME}/include
LIB_CLIENT := -L${ESetSTORE_HOME}/lib 

LIBS = -pthread -l${libECClient}

CFLAGS = -Wall $(OPTFLAGS) $(LIB_CLIENT) -L/usr/local/lib ${INCLUDES} $(LIBS)
 
TARGET = ECClientFS
OBJS = configProperties.o cfgParser.o ecUtils.o ECClientFS.o PutIO.o DeleteIO.o GetIO.o

ALL = $(OBJS) $(TARGET)
all: $(ALL)

.SUFFIXES: .c .cu .o

ECClientFS: $(OBJS)
	$(CC) -o ECClientFS $(OBJS) $(CFLAGS)

configProperties.o: configProperties.h
cfgParser.o: cfgParser.h configProperties.h
ECClientFS.o: cfgParser.h configProperties.h ECStoreClientEnv.h
ecUtils.o: ecUtils.h
PutIO.o: PutIO.h
DeleteIO.o: DeleteIO.h
GetIO.o: GetIO.h

.PHONY : clean
clean:
	-rm -rf ECClientFS *.o

install:
	mkdir -p ${ESetSTORE_HOME}/bin
	cp ECClientFS ${ESetSTORE_HOME}/bin
