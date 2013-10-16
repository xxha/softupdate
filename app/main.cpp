#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "../common/macro.h"

char g_szAppName[MAX_DIR_LEN] = {'\0'};	//Application that will be operated!
volatile bool g_IsDbg = false;	//debug flag

static void *mymemset(char *p, int c, unsigned int len)
{
	for (int i=0; i<len; i++)
		p[i] = (char)c;
	return p;
}

void Usage(char *pName )
{
	if( !pName )
	{
		printf("Invalid process name!\n");
		return;
	}
		
	printf("\n  Invalid argument(s)\n\n");
	printf("  Usage:\n");
	//printf("       %s AppName:\n\n", pName);
	printf("       %s AppName [debug [TCP_PORT [UDP_PORT]]]\n\n", pName);
	printf("	   eg:./softupdate ux400\n");
	printf("	        ./softupdate ux400 debug\n");
	printf("	   	 ./softupdate ux400 debug 12000\n");
	printf("	   	 ./softupdate ux400 debug 12000 11200\n");
	printf("  Build Date:\n       %s %s\n", __DATE__, __TIME__);
}

int main(int argc, char* argv[])
{
	int a = 0;	

	if(argc < 2)
	{
		Usage( argv[0] );
		return 0;
	}

	if(argc >= 2)
	{
		mymemset( g_szAppName, 0, MAX_DIR_LEN );
		//Save the application name
		sprintf( g_szAppName, "%s\0", argv[1] );	  
	}

	if(argc >= 3)
	{
		if(!strcmp(argv[2], "debug"))
		{
			g_IsDbg = true;
			printf("main: The debug is enabled now...\n");
		}
	}

	if(argc >= 4)
	{
		try
		{
			g_iServerTCPPort = atoi(argv[3]);
		}
		catch(...)
		{
			g_iServerTCPPort = SERVER_TCP_PORT;
			printf("main: Get g_iServerTCPPort failed.\n");
		}
	}
	
	if(argc >= 5)
	{
		try
		{
			g_iServerUDPPort = atoi(argv[4]);
		}
		catch(...)
		{
			g_iServerUDPPort = SERVER_UDP_PORT;
			printf("main: Get g_iServerUDPPort failed.\n");
		}
	}

	//FindAllSN();

    	a = StartCommonServer();       
	
	if(!a)
	{
		printf("Softupdate server start failed...\n");
		StopCommonServer();
		return 0;
	}
	
	while(1)
	{
		char exitC = getchar();
		if(('x' == exitC) || ('X' == exitC)) 
		{
			break;
		}
		
		sleep(5);		
	}
	
	StopCommonServer();
	
	printf("Softupdate server exit...\n");
	return 0;
}

