CC = gcc

libESetClient = ESetClient
#test ! -f ${ESetSTORE_HOME}/lib/libESetClientGCRS.so  && libESetClient = ESetClient

INCLUDES = -I. -I${ESetSTORE_HOME}/include
LIB_CLIENT := -L${ESetSTORE_HOME}/lib

LIBS = -pthread -l${libESetClient}

CFLAGS = -Wall $(OPTFLAGS) $(LIB_CLIENT) -L/usr/local/lib ${INCLUDES} $(LIBS)
 
TARGET = ESetClientFS
OBJS = configProperties.o cfgParser.o ecUtils.o ESetClientFS.o PutIO.o DeleteIO.o GetIO.o

ALL = $(OBJS) $(TARGET)
all: $(ALL)

.SUFFIXES: .c .cu .o

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CFLAGS)

configProperties.o: configProperties.h
cfgParser.o: cfgParser.h configProperties.h
ESetClientFS.o: cfgParser.h configProperties.h ECStoreClientEnv.h
ecUtils.o: ecUtils.h
PutIO.o: PutIO.h
DeleteIO.o: DeleteIO.h
GetIO.o: GetIO.h

.PHONY : clean
clean:
	-rm -rf $(TARGET) *.o

install:
	mkdir -p ${ESetSTORE_HOME}/bin
	cp $(TARGET) ${ESetSTORE_HOME}/bin