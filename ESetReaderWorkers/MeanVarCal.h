#ifndef MeanVarCal_h
#define MeanVarCal_h

#include <sys/time.h>
#include <stdio.h>

double getTimeIntervalInMS(struct timeval startTime, struct timeval endTime);

double *timeConsume(int fileNum);
double calMeanValue(double *array, int size);
double calVarValue(double *array, int size);
#endif /* MeanVarCal_h */
