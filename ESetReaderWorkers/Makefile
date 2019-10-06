CC = gcc
LIBS = -pthread -lm
CFLAGS = -Wall $(OPTFLAGS) $(LIBS)  -L/usr/local/lib

TARGET = threadWorker
OBJS = main.o ThreadWorker.o MeanVarCal.o

ALL = $(OBJS) $(TARGET)
all: $(ALL)

.SUFFIXES: .c .cu .o

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CFLAGS)

main.o: ThreadWorker.h
ThreadWorker.o: ThreadWorker.h MeanVarCal.h
MeanVarCal.o: MeanVarCal.h

.PHONY : clean
clean:
	rm -rf $(TARGET) *.o

