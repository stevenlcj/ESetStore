CC = gcc
LIBS = -pthread -lm
CFLAGS = -Wall $(OPTFLAGS) $(LIBS)  -L/usr/local/lib

TARGET = taskRunner
OBJS =  TaskRunner.o  TaskRunnerInstance.o TaskNetUtilities.o MessageBuffer.o strUtils.o

ALL = $(OBJS) $(TARGET)
all: $(ALL)

.SUFFIXES: .c .cu .o

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CFLAGS)

TaskRunner.o: TaskRunnerInstance.h
TaskRunnerInstance.o: TaskRunnerInstance.h TaskRunnerCommon.h
TaskNetUtilities.o: TaskNetUtilities.h
MessageBuffer.o: MessageBuffer.h
strUtils.o: strUtils.h

.PHONY : clean
clean:
	rm -rf $(TARGET) *.o
