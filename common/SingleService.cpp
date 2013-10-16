#include <stdio.h>
#include "SingleService.h"

CSingleService::CSingleService(void)
{
	printf("Enter Constructor StartCommonServer\n");
}


CSingleService::~CSingleService(void)
{
}

bool CSingleService::CreateService()
{	
	printf("Enter CreateService\n");
}

bool CSingleService::CloseService()
{	
	printf("Enter CloseService\n");
}
