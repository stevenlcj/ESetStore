CC = gcc
LIBS = -pthread -lm
CFLAGS = -Wall $(OPTFLAGS) $(LIBS)  -L/usr/local/lib

TARGET = taskDistributor
OBJS = TaskDistributor.o TaskRunner.o TaskGenerator.o TaskDistributorUtilities.o TaskRunnerInstance.o TaskNetUtilities.o TaskListener.o MessageBuffer.o strUtils.o MeanVarCal.o

ALL = $(OBJS) $(TARGET)
all: $(ALL)

.SUFFIXES: .c .cu .o

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CFLAGS)

TaskDistributor.o: TaskRunner.h
TaskRunner.o: TaskRunner.h TaskDistributorCommon.h
TaskDistributorUtilities.o: TaskDistributorUtilities.h
TaskRunnerInstance.o: TaskRunnerInstance.h strUtils.h
TaskNetUtilities.o: TaskNetUtilities.h
TaskListener.o: TaskListener.h 
MessageBuffer.o: MessageBuffer.h
TaskGenerator.o: TaskGenerator.h
strUtils.o: strUtils.h
MeanVarCal.o: MeanVarCal.h

.PHONY : clean
clean:
	rm -rf $(TARGET) *.o
