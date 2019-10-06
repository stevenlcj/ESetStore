#include "MeanVarCal.h"
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#define talloc(type,num) (type *)malloc(sizeof(type)*num)

double *timeConsume(int fileNum){
        double *array = talloc(double, fileNum);
            return array;
}

double getTimeIntervalInMS(struct timeval startTime, struct timeval endTime){
        double timeIntervalInMS = ((double)(endTime.tv_sec - startTime.tv_sec))*1000.0 + ((double)(endTime.tv_usec - startTime.tv_usec))/1000.0;
            return timeIntervalInMS;
}

double calMeanValue(double *array, int size){
        int idx;
            double meanValue = 0.0;
                for (idx = 0; idx < size; ++idx) {
                            meanValue = meanValue + *(array + idx) ;
                                }
                    return (meanValue / (double)size);
}

double calVarValue(double *array, int size){
        double meanValue = calMeanValue(array, size);
            double varValue = 0.0;
                int idx;
                    
                    for (idx = 0; idx < size; ++idx) {
                                varValue = varValue + (meanValue - *(array + idx)) * (meanValue - *(array + idx));
                                    }
                        
                        varValue = sqrt(varValue/(double)size);
                            
                            return varValue;
}

