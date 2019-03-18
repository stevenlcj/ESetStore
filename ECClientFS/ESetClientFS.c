#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cfgParser.h"
#include "configProperties.h"
#include "ECStoreClientEnv.h"
#include "ECClientEngine.h"
#include "PutIO.h"
#include "DeleteIO.h"
#include "GetIO.h"

#define MIN_ARG_SIZE 3

typedef enum{
	CMD_PUT = 0x01,
	CMD_GET = 0x02,
	CMD_RM = 0x03,
	CMD_UNKNOW
}CMD_TYPE;

CMD_TYPE getCmdType(char *cmdStr){
	if(strncmp(cmdRm, cmdStr, strlen(cmdRm)) == 0){
		return CMD_RM;
	}else if(strncmp(cmdGet, cmdStr, strlen(cmdGet)) == 0){
		return CMD_GET;
	}else if(strncmp(cmdPut, cmdStr, strlen(cmdPut)) == 0){
		return CMD_PUT;
	}
	
	return CMD_UNKNOW;
}

void printCmdType(CMD_TYPE cmdType){
	switch(cmdType){
		case CMD_PUT:
			printf("Put cmd\n");
		break;
		case CMD_GET:
			printf("Get cmd\n");
		break;
		case CMD_RM:
			printf("Rm cmd\n");
		break;
		default:
			printf("Unknow cmd\n");
	}
}

void usage(){
	printf("Please specify cmd:-put,-get, -rm and files to handle \n");
}

int main(int argc, char *argv[]){
	if(argc  < MIN_ARG_SIZE){
		usage();
		exit(-1);
	}
	
	CMD_TYPE cmdType = getCmdType(argv[1]);
	if (cmdType == CMD_UNKNOW){
		printf("Unknow cmd\n");
        exit(-1);		
	}

	int fileIdx, fileNum = argc - 2;
    char **filePtrs = (char **)malloc(sizeof(char *) * (argc - 2));

    for (fileIdx = 0; fileIdx < fileNum; ++fileIdx)
    {
    	filePtrs[fileIdx] = argv[fileIdx + 2];
    }

	for (fileIdx = 0; fileIdx < fileNum; ++fileIdx)
    {
    	printf("%s\n",filePtrs[fileIdx]);
    }
	char cfgFile[50]="\0";
	char *ecStoreHomePath = getenv(ESetStoreHome);
	
	strcat(cfgFile,ecStoreHomePath);
	strcat(cfgFile,cfgFileSuffix);
	
	//printf("ECStore cfg file path:%s*str len:%lu\n",cfgFile, strlen(cfgFile));
	
	struct configProperties *cfgPropties= loadProperties(cfgFile);
	
	char *MetaIPAddr = getStrValue(cfgPropties, ESetStoreMetaServer_IP);
	int MetaPort = getIntValue(cfgPropties, ESetStoreMetaServer_Port);
	
	//printf("MetaServer IP addr:%s, port:%d\n", MetaIPAddr, MetaPort);
	
	ECClientEngine_t *clientEngine = createECClientEngine(MetaIPAddr, MetaPort);

	if(clientEngine == NULL){
		printf("Error: unable to connect meta server\n");
		exit(-1);
	}
    
    switch(cmdType){
    	case CMD_PUT:
    		startPutWork(clientEngine, filePtrs, fileNum);
    	break;
    	case CMD_RM:
    		startDeleteWork(clientEngine, filePtrs, fileNum);
    	break;
    	case CMD_GET:
    		startGetWork(clientEngine, filePtrs, fileNum);
    	default:
    	break;
    }

	deallocECClientEngine(clientEngine);
	return 0;
}