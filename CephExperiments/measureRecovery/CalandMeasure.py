import os
import sys
import time
import datetime
from timeit import default_timer as timer

if len(sys.argv) < 1:
    print 'Please specify the osd to cal'
    exit()
process = os.popen("sudo ceph pg dump | awk '{print $1,$2,$4,$10,$15,$16,$17,$18}'") # return file
output = process.read()
process.close()
oStrs = output.splitlines(True)
eachLineSize=8
lineNum=0
totalObjects=0
degradedObjects=0
eachBlockSize=64.00
osdId=str(sys.argv[1])
osdIdL="["+osdId+","
osdIdM=","+osdId+","
osdIdR=","+osdId+"]"

for line in oStrs:
    if "active" in line:
        lineStrs=line.split()
	arraySize = len(lineStrs)
        totalObjects=totalObjects + int(lineStrs[1])
        if osdIdL in lineStrs[4] or osdIdM in lineStrs[4] or osdIdR in lineStrs[4] :
            print osdId + " in " + lineStrs[4]
            degradedObjects = degradedObjects + int(lineStrs[1])
print "Total objects:"+str(totalObjects) + " Degraded objects:" + str(degradedObjects)

notInRecovery=1
startTime=timer()
sleepInterval=0.1
sizePerObject=4.0

while notInRecovery==1:
    process = os.popen("sudo ceph -s") # return file
    output = process.read()
    process.close()
    oStrs = output.splitlines(True)
    for line in oStrs:
        if "recovering" in line:
            startTime=timer()
            notInRecovery=0
            print line
    if notInRecovery==0:
        break
    time.sleep(sleepInterval)

endTime=timer()

recoveryNotOver=1

while recoveryNotOver==1:
    process = os.popen("sudo ceph -s") # return file
    output = process.read()
    process.close()
    oStrs = output.splitlines(True)
    for line in oStrs:
        if "pgs" in line and "active+clean" in line:
            endTime=timer()
            recoveryNotOver=0
    if recoveryNotOver==0:
        break
    time.sleep(sleepInterval)

timeConsume=endTime-startTime
throughput=(sizePerObject * degradedObjects)/timeConsume
print "Time consume:"+str(timeConsume) + "Throughput:"+str(throughput)+"MB/s"


#print output
