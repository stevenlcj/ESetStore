import os
import sys

from timeit import default_timer as timer

if len(sys.argv) < 3:
    print 'Please specify the startIdx endIdx endIdx'
    exit()

fileSize = 0
fileStartIdx = int(sys.argv[1])
fileEndIdx = int(sys.argv[2])
filePrefixName = "file"
cmd = "ECClientFS -get "
my_path="./"

for fileIdx in range(fileStartIdx, fileEndIdx+1):
    fileName = filePrefixName + str(fileIdx)
    cmd = cmd + fileName + " "

start = timer()	
cmd= cmd
os.system(cmd)
end = timer()

for fileIdx in range(fileStartIdx, fileEndIdx+1):
    fileSize = fileSize + os.path.getsize(my_path+fileName)
print cmd
timeConsume = (end - start)
throughput = (float(fileSize)/1024.0/1024.0)/float(timeConsume)
fileSizesInMB = float(fileSize)/1024.0/1024.0

print "File num:"+str(fileIdx)+" Total size:"+str(fileSizesInMB)+ "MB Write time consume:" + str(timeConsume) + " Throughput:"+str(throughput)+"MB/s"
