
//
//  DeleteIO.c
//  ECClientFS
//
//  Created by Liu Chengjian on 18/4/23.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "DeleteIO.h"

void startDeleteWork(ECClientEngine_t *clientEngine, char *filesToDelete[], int fileNum){
//	int idx;
//	int ret;
	/*for (idx = 0; idx < fileNum; ++idx)
	{
		ret = deleteFile(clientEngine, filesToDelete[idx]);

		if (ret != 0)
		{
			printf("Delete failed for file:%s\n", filesToDelete[idx]);
		}
	}*/

	deleteFiles(clientEngine, filesToDelete, fileNum);
}