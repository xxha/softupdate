/*****************************************************************************************
** VControlServer.cpp: provide external app, start or end service, control service routine.
** routine:
**	 1. external app call StartCommonServer() in VControlServer.cpp, to start listen service.
**	 2. create new thread, to start TCP listen.
**	 3. remote IP connect request approaching, then create new thread, and new CCommonServer instance, and input Socket .
**	 4. instances object of CCommonServer interact with remote side, and communication
**
**
** Author: cjiang
** Copyright (C) 2012 VeTronics(BeiJing) Ltd.
**
** Create date: 05-04-2012
*****************************************************************************************/

#include <netinet/in.h>					// for sockaddr_in 
#include <sys/types.h>					// for socket 
#include <sys/socket.h>					// for socket 
#include <sys/select.h>
#include <sys/time.h>
#include <stdio.h>					// for printf 
#include <stdlib.h>					// for exit 
#include <string.h>					// for bzero 
#include <stdarg.h>					//va_start,va_list
#include <pthread.h>					//pthread_t,pthread_exit,pthread_create
#include <arpa/inet.h>
#include <unistd.h>					//sleep()
#include <sys/stat.h> 
#include <dirent.h>					//DIR
#include <fcntl.h> 
#include <ctype.h>					//isalnum
#include <ftdi.h>
#include "macro.h"
#include "CommonServer.h"
#include "../v400eeprom/v400eeprom.h"
#include "../clientbase/md5checksum.h"

#define	UX400VENDOR	0x0403
#define	UX400PRODUCT	0x6010

#define	EEPROM_SIZE		128

#define	OPTION_OFFSET_FTDI	 0x70
#define	OPTION_SIZE_FTDI		14


struct ftdi_context ux400_ftdic;
unsigned char eeprom[EEPROM_SIZE];
char testSN[256] = {'\0'};

pthread_t threadHandleListen;					//service thread for listen
pthread_t threadHandleBroadcastListen;				//service thread for boardcast listin
bool StopAll = false;

//port number with module index
char g_szArg_netif[64] = "";
int  g_iArg_srvport	= 8023;
char g_szArg_optfile[256] = "";
struct sockaddr_in g_inAddrServer;
int g_iServerTCPPort = SERVER_TCP_PORT;
int g_iServerUDPPort = SERVER_UDP_PORT;

int DebugPrintfArray(char * strName,UCHAR *array,int iLen)
{
	if(NULL != array)
	{
		DebugPrintf("%s:\n",strName);
		for(int i = 0; i < iLen; i++)
		{
			DebugPrintf("%02X ",array[i]);
			if((i + 1) % 16 == 0)
			{
				DebugPrintf("\n");			
			}
		}
		DebugPrintf("\nArray finish\n");
	}
}

 int DebugPrintf(char *format, ...)
{	
	char szBuf[256] = {'\0'};
	va_list args;
	extern bool g_IsDbg;

	if( true == g_IsDbg )
	{
		va_start(args, format);	
		vsnprintf(szBuf, 255, format, args);

		printf(szBuf);
		return 0;
	}

	return 0;
}

void* CreatCommonServerInstance(void* socketClient)
{
	DebugPrintf("Into CreatCommonServerInstance Success! socket = %d .\n",*((int *)(socketClient)));
	pthread_detach(pthread_self());

	CCommonServer* css = new CCommonServer();
	int error = css->SetSocket(*((int *)(socketClient)));
	if(0 == error)
	{
		//Success
		DebugPrintf("CreatCommonServerInstance: SetSocket success!\n"); 

		css->Connect();

		while(css->GetIsConnection())
		{
			sleep(5);
			//DebugPrintf("sleep 5......\n"); 

			if(StopAll)
			{
				break;
			}
		}
	}
	else
	{
		//Error
		DebugPrintf("CreatCommonServerInstance: SetSocket failed! Error = % d \n",error);
	}
	
	css->Disconnect();
}

//create CCommonServer instance, and start it's TCP and UDP listen service.
void* BeginMySocketListen(void*)
{
	DebugPrintf("Into BeginMyTcpSocket...\n");
	 
	//set server_addr for a socket struct. that means internet address and port number of service 
	struct sockaddr_in serverListen_addr; 
	bzero(&serverListen_addr,sizeof(serverListen_addr));
	serverListen_addr.sin_family = AF_INET; 
	serverListen_addr.sin_addr.s_addr = htons(INADDR_ANY); 
	serverListen_addr.sin_port = htons(g_iServerTCPPort); 

	//create stream protocal(TCP) socket on server.  
	int serverListen_socket = socket(PF_INET,SOCK_STREAM,0); 
	if( serverListen_socket < 0) 
	{ 
		printf("BeginMySocketListen: Create Socket Failed!\n"); 
		exit(1); 
	}
	else
	{
		DebugPrintf("BeginMySocketListen: Create Socket Success!\n");
	}
	 
	//bind socket and socket address 
	if(bind(serverListen_socket,(struct sockaddr*)&serverListen_addr,sizeof(serverListen_addr))) 
	{ 
		printf("BeginMySocketListen: Server Bind Port : %d Failed!\n", g_iServerTCPPort);  
		exit(1); 
	}
	else
	{
		DebugPrintf("BeginMySocketListen: Server Bind Port : %d Success!\n", g_iServerTCPPort);
	}
	 
	//serverListen_socket for listen
	if (listen(serverListen_socket, LENGTH_OF_LISTEN_QUEUE) ) 
	{
		printf("BeginMySocketListen: Server Listen Failed!");  
		exit(1); 
	}
	else
	{
		DebugPrintf("BeginMySocketListen: Server Listen Success!\n");
	}
	
	int iCount = 0;
	fd_set	set;
	struct	  timeval timeout;
	socklen_t   iAddrSize;
	while(1)
	{
		if(StopAll)
		{
			printf("BeginMySocketListen:  Stop signal received, begi stopping thread...\n");
			break;
		}
		FD_ZERO(&set);
		FD_SET(serverListen_socket,&set);

		timeout.tv_sec=NK_SVR_TIMETOCHECK;
		timeout.tv_usec=0;
		iAddrSize=sizeof(struct sockaddr);
		int ret = select(serverListen_socket+1, &set, NULL, NULL,&timeout);
		if(!ret || SOCKET_ERROR==ret)
		{
			continue;
		}
		else
		{
			iCount++;
			DebugPrintf("BeginMySocketListen:\r\n..New id = %d ....\r\n",iCount);

			//client socket address 
			struct sockaddr_in clientDest_addr; 
			socklen_t length = sizeof(clientDest_addr); 

			//接受一个到serverListen_socket代表的socket的一个连接 
			//如果没有连接请求,就等待到有连接请求--这是accept函数的特性 
			//accept函数返回一个新的socket,这个socket(newClient_socket)用于同连接到的客户的通信 
			//newClient_socket代表了服务器和客户端之间的一个通信通道 
			//accept函数把连接到的客户端信息填写到客户端的socket地址结构client_addr中 
		
			DebugPrintf("BeginMySocketListen: Server waiting client......!\n");
			int newClient_socket = accept(serverListen_socket,(struct sockaddr*)&clientDest_addr,&length);
			if ( newClient_socket < 0) 
			{ 
				printf("BeginMySocketListen: Server Accept Failed!\n"); 
				break; 
			}
			else
			{
				DebugPrintf("BeginMySocketListen: Server Accept Success! IP = %s is connect.\n",inet_ntoa(clientDest_addr.sin_addr));			
			}

			//连接
			pthread_t tempThread;
			if(!pthread_create(&tempThread, NULL, CreatCommonServerInstance, &newClient_socket))
			{			
				printf("BeginMySocketListen: pthread_create create CommonServer Success! IP = %s ,socket = %d .\n",inet_ntoa(clientDest_addr.sin_addr),newClient_socket);
			}
			else
			{			
				DebugPrintf("BeginMySocketListen: pthread_create create CommonServer failed! IP = %s ,socket = %d .\n",inet_ntoa(clientDest_addr.sin_addr),newClient_socket);
			}
		} 
	}
	//close listen socket
	close(serverListen_socket); 

	DebugPrintf("BeginMySocketListen: Leaveing BeginMyTcpSocket...\n");
	threadHandleListen = 0;
	return NULL;
}

//start listen broadcast
void* BeginMySocketBroadcastListen(void*)
{
	DebugPrintf("Into BeginMySocketBroadcastListen...\n");
	pthread_detach(pthread_self());
	 
	//设置一个socket地址结构server_addr,代表服务器internet地址, 端口 
	struct sockaddr_in serverBroadcast_addr; 
	bzero(&serverBroadcast_addr,sizeof(serverBroadcast_addr)); //把一段内存区的内容全部设置为0 
	serverBroadcast_addr.sin_family = AF_INET; 
	serverBroadcast_addr.sin_addr.s_addr = htons(INADDR_ANY); 
	serverBroadcast_addr.sin_port = htons(g_iServerUDPPort); 
	struct sockaddr_in addrClient;
	bzero(&addrClient,sizeof(addrClient));

	//创建用于internet的流协议(TCP)socket,用serverListen_socket代表服务器socket 
	int serverBroadcast_socket = socket(PF_INET,SOCK_DGRAM,0); 
	if( serverBroadcast_socket < 0) 
	{ 
		printf("BeginMySocketBroadcastListen: Create UDP Socket Failed!\n"); 
		exit(1); 
	}
	else
	{
		DebugPrintf("BeginMySocketBroadcastListen: Create UDP Socket Success!\n");
	}

	bool optval=true; 
	setsockopt(serverBroadcast_socket,SOL_SOCKET,SO_BROADCAST,(char*)&optval,sizeof(bool));
	 
	//把socket和socket地址结构联系起来 
	if(bind(serverBroadcast_socket,(struct sockaddr*)&serverBroadcast_addr,sizeof(serverBroadcast_addr))) 
	{ 
		printf("BeginMySocketBroadcastListen: Server Bind Port : %d Failed!\n", g_iServerUDPPort);  
		exit(1); 
	}
	else
	{
		DebugPrintf("BeginMySocketBroadcastListen: Server Bind Port : %d Success!\n", g_iServerUDPPort);
	}
	
	int iCount = 0;
	fd_set	set;
	struct	  timeval timeout;
	socklen_t   iAddrSize;
	while(1)
	{
		if(StopAll)
		{
			printf("BeginMySocketBroadcastListen:  Stop signal received, begi stopping thread...\n");
			break;
		}
		FD_ZERO(&set);
		FD_SET(serverBroadcast_socket,&set);

		timeout.tv_sec=NK_SVR_TIMETOCHECK;
		timeout.tv_usec=0;
		iAddrSize=sizeof(struct sockaddr);
		int ret = select(serverBroadcast_socket+1, &set, NULL, NULL,&timeout);
		if(!ret || SOCKET_ERROR==ret)
		{
			continue;
		}
		else
		{
			iCount++;
			DebugPrintf("BeginMySocketBroadcastListen: \r\n..New UDP id = %d ....\r\n",iCount);
		
			socklen_t length = sizeof(addrClient); 
			// 从客户端接收数据
			char buffer[BUFFER_SIZE]; 
			bzero(buffer, BUFFER_SIZE); 
			int nRet = recvfrom(serverBroadcast_socket,buffer,BUFFER_SIZE,0,(struct sockaddr*)&addrClient,&length);	
			if(SOCKET_ERROR == nRet)	   
			{
				printf("BeginMySocketBroadcastListen: recvfrom failed.Error = %d !\n",nRet);
				continue;
			}
			// 打印来自客户端发送来的数据
			DebugPrintf("BeginMySocketBroadcastListen: Recv From Client:%s,ip = %s\n",buffer,inet_ntoa(addrClient.sin_addr));

			if(!strcmp("SOFTUPDATE400CAST",buffer))
			{
				int iRet = 0;
				unsigned char pSerNumber[14] = {'\0'}; 
				if(iRet = ReadSerialNumber(pSerNumber))
				{
					printf("BeginMySocketBroadcastListen: Read Serial No from flash fail, errno:%d!\n", iRet);
				}
				else
				{
					DebugPrintf("BeginMySocketBroadcastListen: Serial Number: %s \n",pSerNumber);
				}

				// 向客户端发送数据
				bzero(buffer, BUFFER_SIZE);
				sprintf(buffer, "%s%s", "SOFTUPDATE400CAST:",pSerNumber);
				addrClient.sin_port = htons(g_iServerUDPPort); 
				sendto(serverBroadcast_socket,buffer,strlen(buffer),0,(struct sockaddr*)&addrClient,length); 
			}
		} 
	}
	//关闭监听用的socket 
	close(serverBroadcast_socket); 
	threadHandleBroadcastListen = 0;
	return NULL;
}

int recv_timeout(int skClient, void *buff, int size, int to_usecs)
{
	int  recv_size=-1;
	struct timeval recv_wait;
	fd_set		 recv_fdset;

	if (to_usecs <= 0)
	{
		recv_wait.tv_sec  = 0;
		recv_wait.tv_usec = 0;
	}
	else
	{
		recv_wait.tv_sec  = to_usecs / MILLION;
		recv_wait.tv_usec = to_usecs % MILLION;
	}

	FD_ZERO(&recv_fdset);
	FD_SET(skClient, &recv_fdset);

	select(skClient+1, &recv_fdset, NULL, NULL, &recv_wait);
	if (FD_ISSET(skClient, &recv_fdset))
	{
		recv_size = recv(skClient, buff, size, 0);
	}

	return recv_size;
}

//-1: v400_boardres_ind2usbpath:failed
//end
int v400_boardres_ind2usbpath (int board_index,int* pusb_bus, int* pusb_topport, int* pusb_subport)
{
	int  flag_err = 0;
	int  iUSBBus=-1, iTopPort=-1, iSubPort=-1;

	/* evaluate USB bus and port index */
	switch (board_index)
	{
		case 0:
			iUSBBus  = 1;
			iTopPort = 1;
			iSubPort = 2;
			break;
		case 1:
			iUSBBus  = 1;
			iTopPort = 5;
			iSubPort = -1;
			break;
		case 2:
			iUSBBus  = 1;
			iTopPort = 1;
			iSubPort = 3;
			break;
		case 3:
			iUSBBus  = 1;
			iTopPort = 3;
			iSubPort = -1;
			break;
		case 4:
			iUSBBus  = 1;
			iTopPort = 1; 
			iSubPort = 4;
			break;
		case 5:
			iUSBBus  = 1;
			iTopPort = 1;
			iSubPort = 1;
			break;
		default:
			iUSBBus  = -1;
			iTopPort = -1;
			iSubPort = -1;
			flag_err = 1;
	}

	/* fill returning values */
	if (pusb_bus != NULL)
		*pusb_bus = iUSBBus;
	if (pusb_topport != NULL)
		*pusb_topport = iTopPort;
	if (pusb_subport != NULL)
		*pusb_subport = iSubPort;
	return (flag_err == 0) ? 0 : -1;
}

//-1:	v400_boardres_ind2sn:failed
//11:	v400_boardres_ind2sn:check parameters  failed
//21:	v400_boardres_ind2sn:check whether device exist failed
//end
int v400_boardres_ind2sn (int board_index, char *sn)
{
	DebugPrintf("in v400_boardres_ind2sn.....\n");
	
	int  ret_val = 0;
	int  iUSBBus=-1, iTopPort=-1, iSubPort=-1;
	char szBoardSN[32]="";
	char szUSBPath[256]="";
	char szTemp[256]="";

	/* init returning values */
	if (sn != NULL)
		sn[0] = '\0';

	/* check parameters */
	ret_val = ((board_index < 0) || (board_index > V400PANEL_BOARDNUM)) ? 11 : ret_val;
	if(0 != ret_val)
	{
		return ret_val;
	}

	/* check whether device exist */
	ret_val = (v400_boardres_ind2usbpath(board_index,
				&iUSBBus, &iTopPort, &iSubPort) != 0) ? 21 : ret_val;
	if(0 != ret_val)
	{
		return ret_val;
	}

	if (iSubPort < 0)
		sprintf(szUSBPath, "/sys/bus/usb/devices/%d-%d",
								iUSBBus, iTopPort);
	else
		sprintf(szUSBPath, "/sys/bus/usb/devices/%d-%d.%d",
								iUSBBus, iTopPort, iSubPort);
	if (access(szUSBPath, F_OK) != 0)
	{
		if(0 != ret_val)
		{
			return ret_val;
		}
	}

	/* read board S/N definition */
	int   fdSerial=-1;
	bzero(szBoardSN, sizeof(szBoardSN));

	sprintf(szTemp, "%s/serial", szUSBPath);
	if ((fdSerial = open(szTemp, O_RDONLY)) > 0)
	{
		int read_ret;
		read_ret = read(fdSerial, szBoardSN, 16);
		close(fdSerial);
	}
	
	/* regulate the S/N that stored in szBoardSN */
	int  iByteIndex=0;
	while (iByteIndex < sizeof(szBoardSN))
	{
		if (isalnum(szBoardSN[iByteIndex]))
			iByteIndex ++;
		else
		{
			bzero(&szBoardSN[iByteIndex], sizeof(szBoardSN)-iByteIndex);
			break;
		}
	}

	/* fill returning values */
	if (sn != NULL)
	{
		memcpy(sn, szBoardSN, 16);
		sn[16] = '\0';
	}
	
	DebugPrintf("v400_boardres_ind2sn: sn = %s,szBoardSN = %s.\n",sn,szBoardSN);

	return (ret_val == 0) ? 0 : -1;
}

int v400_boardres_sn2type (const void *sn)
{
	int iBoardType=eV400_board_unknown;

	if (sn == NULL)
		iBoardType = eV400_board_unknown;
	else if (memcmp(sn, "BVUX", 4) == 0)
		iBoardType = eV400_board_40g;
	else if (memcmp(sn, "BVUM", 4) == 0)
		iBoardType = eV400_board_40ge;
	else if (memcmp(sn, "BVUP", 4) == 0)
		iBoardType = eV400_board_16g;
	else if (memcmp(sn, "BVUK", 4) == 0)
		iBoardType = eV400_board_10g;
	else if (memcmp(sn, "BVUJ", 4) == 0)
		iBoardType = eV400_board_2g5;
	else if (memcmp(sn, "BVUL", 4) == 0)
		iBoardType = eV400_board_1ge;
	else if (memcmp(sn, "BVUH", 4) == 0)
		iBoardType = eV400_board_100ge;
	else
		iBoardType = eV400_board_unknown;

	return iBoardType;
}

int GetBoardresType(int index)
{
	int iType = 0;
	char sn[256] = {'\0'};
	if(0 == v400_boardres_ind2sn(index,sn))
	{
		DebugPrintf("OnReceived.MSG_GETBOARDTYPE: sn = %s\n",sn);
		
		iType = v400_boardres_sn2type(sn);
	}
	else
	{
		printf("OnReceived.MSG_GETBOARDTYPE:v400_boardres_ind2sn failed.");
	}

	return iType;
}

//
//-1: v400_boardres_ind2usbpath:failed
//7: 	v400_boardres_ind2netif:Get szNetifPath failed
//8:		v400_boardres_ind2netif:read interface name to szNetifName failed
//9:		v400_boardres_ind2netif:szNetifName is empty
//10:  	v400_boardres_ind2netif:failed
//end
int v400_boardres_ind2netif (int board_index, char* netif)
{
	int  ret_val = 0;
	int  iUSBBus=-1, iTopPort=-1, iSubPort=-1;
	DIR *pdir_net=NULL;
	struct dirent *pent_dir=NULL;
	int  flag_netaddr=0;
	char szNetifName[64]="";
	char szNetifPath[256]="";
	char szTemp[256];

	/* init returning values */
	if (netif != NULL)
		netif[0] = '\0';
	szNetifName[0] = '\0';
	szNetifPath[0] = '\0';

	/* read the path to szNetifPath */
	ret_val = (v400_boardres_ind2usbpath(board_index,&iUSBBus, &iTopPort, &iSubPort) != 0) ? 6 : ret_val;
	if(0 != ret_val)
	{
		return ret_val;
	}

	int  bPathDone=0;
	int  iUSBConfig, iUSBIntf;
	for (iUSBConfig=1; (bPathDone == 0) && (iUSBConfig <= 1); iUSBConfig++)
	{
		for (iUSBIntf=0; (bPathDone == 0) && (iUSBIntf <= 1); iUSBIntf++)
		{
			if (iSubPort < 0)
				sprintf(szTemp, "/sys/bus/usb/devices/%d-%d/%d-%d:%d.%d/net",
								iUSBBus, iTopPort, iUSBBus, iTopPort, iUSBConfig, iUSBIntf);
			else
				sprintf(szTemp, "/sys/bus/usb/devices/%d-%d.%d/%d-%d.%d:%d.%d/net",
								iUSBBus, iTopPort, iSubPort, iUSBBus, iTopPort, iSubPort, iUSBConfig, iUSBIntf);
			if (access(szTemp, F_OK) == 0)
			{
				bPathDone = 1;
				strcpy(szNetifPath, szTemp);
			}
		}
	}

	ret_val = (bPathDone == 0) ? 7 : ret_val;			

	if(0 != ret_val)
	{
		return ret_val;
	}

	/* read interface name to szNetifName */
	pdir_net = opendir(szNetifPath);
	ret_val = (pdir_net == NULL) ? 8 : ret_val;
	
	if(0 != ret_val)
	{
		return ret_val;
	}

	while ((szNetifName[0] == '\0') && ((pent_dir = readdir(pdir_net)) != NULL))
	{
		struct stat stat_tmp;
		if ((pent_dir->d_name[0] == '\0') || (pent_dir->d_name[0] == '.'))
			continue;
		sprintf(szTemp, "%s/%s", szNetifPath, pent_dir->d_name);
		if (stat(szTemp, &stat_tmp) != 0)
			continue;
		if (!S_ISDIR(stat_tmp.st_mode))
			continue;
		sprintf(szTemp, "/sys/class/net/%s", pent_dir->d_name);
		if (access(szTemp, F_OK) != 0)
			continue;

		strncpy(szNetifName, pent_dir->d_name, sizeof(szNetifName));
		szNetifName[sizeof(szNetifName)-1] = '\0';
	}
	ret_val = (szNetifName[0] == '\0') ? 9 : ret_val;
	
	if(0 != ret_val)
	{
		return ret_val;
	}
	
	/* fill returning values */
	if (netif != NULL)
		strcpy(netif, szNetifName);

	if (pdir_net != NULL)
		closedir(pdir_net);
	return (ret_val == 0) ? 0 : 10;
}

//15:	_netclient_cmd_opt:Buffer is NULL
//20:	_netclient_cmd_opt:cmd invalid
//16:	_netclient_cmd_opt:send cmd(%s) failed!
//17:	_netclient_cmd_opt:recv cmd(%s)  netif(%s) failed!
//18:	_netclient_cmd_opt:read cmd(%s) options failed!
//end
//用来发送命令,返回接收到的字符串
int _netclient_cmd_opt(int skClient,char *cmd,int size_send,char temp_buff[],bool bIsWrite)
{
	DebugPrintf("in _netclient_cmd_opt:cmd = %s,size_send = %d. \n",cmd,size_send);

	int  ret_val=0;
	int  size_recv=0;
	if(NULL == temp_buff)
	{
		return 15;			//Buffer is NULL
	}
	temp_buff[0] = '\0';

	if(NULL == cmd)
	{
		return 20;			//cmd invalid
	}

	size_send = sendto(skClient, cmd, size_send, 0,(const struct sockaddr*)&g_inAddrServer, sizeof(g_inAddrServer));
	if (size_send <= 0)
	{
		printf("_netclient_cmd_opt: send cmd(%s) failed! \n",cmd);
		ret_val = 16;
		return ret_val;
	}

	/* receive feedback */
	size_recv = recv_timeout(skClient, temp_buff, 4095, (bIsWrite?MILLION * 60:MILLION));
	if (size_recv <= 0)
	{
		printf("_netclient_cmd_opt: recv cmd(%s)  netif(%s) failed! \n",cmd, g_szArg_netif);
		ret_val = 17;
		return ret_val;
	}
	else
	{
		temp_buff[size_recv] = '\0';
	}

	if (memcmp(temp_buff, "ERR:", 4) == 0)
	{
		//write(2, temp_buff, size_recv);
		printf("_netclient_cmd_opt: read cmd(%s) options failed!ERR:%s .\n",cmd,temp_buff);
		ret_val = 18;
		return ret_val;
	}
	DebugPrintf("_netclient_cmd_opt: cmd(%s): %s,size_recv = %d.\n",cmd,temp_buff,size_recv);

	return (ret_val == 0) ? 0 : 18;
}

//15:	_netclient_cmd_ReadEEPRom:Buffer is null

//16:	_netclient_cmd_opt:send cmd(%s) failed!
//17:	_netclient_cmd_opt:recv cmd(%s)  netif(%s) failed!
//18:	_netclient_cmd_opt:read cmd(%s) options failed!
//20:	_netclient_cmd_opt:cmd invalid
//end
int _netclient_cmd_ReadEEPRom(int skClient ,char pBuff[])
{
	DebugPrintf("in _netclient_cmd_ReadEEPRom:  .\n");

	char cmd[1024] = {'\0'};
	if((NULL == cmd) ||(NULL == pBuff))
	{
		return 15;			//Buffer is NULL
	}
	cmd[0] = '\0';
	pBuff[0] = '\0';
	
	sprintf(cmd,"%s","opthex");
	int size_send = strlen(cmd);
	cmd[size_send] = '\0';

	return _netclient_cmd_opt(skClient,cmd,size_send,pBuff,false);	
}

//15:	_netclient_cmd_WriteEEPRom:Buffer is NULL
//15:	_netclient_cmd_opt:Buffer is NULL
//20:	_netclient_cmd_opt:cmd invalid
//16:	_netclient_cmd_opt:send cmd(%s) failed!
//17:	_netclient_cmd_opt:recv cmd(%s)  netif(%s) failed!
//18:	_netclient_cmd_opt:read cmd(%s) options failed!
//end
//写带索引的模块内容
int _netclient_cmd_WriteEEPRom(int skClient ,int iStart,int iLen,UCHAR pBuff[])
{
	DebugPrintf("in _netclient_cmd_system ,pBuff = %s,iStart = %d.\n",pBuff,iStart);
	DebugPrintfArray("_netclient_cmd_system:pBuff", pBuff, iLen);

	char cmd[1024] = {'\0'};
	if((NULL == cmd) ||(NULL == pBuff))
	{
		return 15;			//Buffer is NULL
	}
	memset(cmd,0,1024);

	char buff[EEPROM_MAXSIZE * 2] = {'\0'};
	for(int i = 0; i < iLen; i++)
	{
		sprintf(buff + i * 2,"%02X",pBuff[i]);
	}
	buff[iLen * 2] = '\0';
	
	sprintf(cmd,"%s %d %s","optfill --begin",iStart,buff);
	int size_send = strlen(cmd);
	cmd[size_send] = '\0';
	char temp_buf[SIZE_EEPRom] = {0};
	
	return _netclient_cmd_opt(skClient,cmd,size_send,temp_buf,true);
}

//11:		CreateModuleSocketClient:no network interface specified
//12:		CreateModuleSocketClient:network interface * doesn't exist
//13:		CreateModuleSocketClient:Create socket failed
//14:		CreateModuleSocketClient:Set socketopt failed
//-1:		CreateModuleSocketClient:failed
//end
//读取带索引的模块内容,创建socket
int CreateModuleSocketClient(int *pskClient)
{
	DebugPrintf("in CreateModuleSocketClient:  \n");

	int  ret_val = 0;
	*pskClient = -1;
	int  cmd_index=-1;
	int  size_send, size_recv;
	char temp_buff[1024];
	
	if (NULL == g_szArg_netif)
	{
		fprintf(stderr, "CreateModuleSocketClient: ERR: no network interface specified!\n");
		ret_val = 11;
		return ret_val;
	}

	if (g_szArg_netif[0] == '\0')
	{
		fprintf(stderr, "CreateModuleSocketClient: ERR: no network interface specified!\n");
		ret_val = 11;
		return ret_val;
	}

	/* wait network device to be ready */
	sprintf(temp_buff, "/sys/class/net/%s", g_szArg_netif);
	if (access(temp_buff, F_OK) != 0)
	{
		fprintf(stderr, "ERR: network interface(%s) doesn't exist!\n",g_szArg_netif);
		ret_val = 12;
		return ret_val;
	}

	bzero(&g_inAddrServer, sizeof(g_inAddrServer));
	g_inAddrServer.sin_family = AF_INET;
	g_inAddrServer.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	g_inAddrServer.sin_port   = htons(g_iArg_srvport);

	*pskClient = socket(AF_INET, SOCK_DGRAM, 0);
	ret_val = (*pskClient < 0) ? 13 : ret_val;
	if(0 != ret_val)
	{
		return ret_val;
	}

	try
	{
		int bOptOn=1;
		setsockopt(*pskClient, SOL_SOCKET, SO_BROADCAST, &bOptOn, sizeof(bOptOn));
		setsockopt(*pskClient, SOL_SOCKET, SO_BINDTODEVICE, g_szArg_netif, strlen(g_szArg_netif));
	}
	catch(...)
	{
		ret_val = 14;
	}

	return (ret_val == 0) ? 0 : -1;
}

//3:		ReadOptionBuffer:Length invalid
//4:		ReadOptionBuffer:eeprom_open failed
//5:		ReadOptionBuffer:eeprom_read_buf failed
//end
int ReadOptionBuffer(unsigned char *pBuff, int start,int iLen)
{
	DebugPrintf("in ReadOptionBuffer: No index: start = %d,iLen = %d.\n",start,iLen);

	int iRet = 0;
	BYTE bFlag = 0;
	
	if((iLen > E2PROM_OPTION_SIZE) || (iLen <= 0))
	{
			printf("ReadOptionBuffer: iLen is invalid.\n");
		return 3;	//Length invalid
	}
	
	iRet = eeprom_open();
	if( -1 == iRet)
	{
		printf("ReadOptionBuffer: eeprom_open failed.\n");
		return 4;		//eeprom_open failed
	}
	
	iRet = eeprom_read_buf(start, iLen, pBuff);	
	eeprom_close();

	DebugPrintfArray("ReadOptionBuffer", pBuff, iLen);
	
	DebugPrintf("ReadOptionBuffer: ReadOptionBuffer:Finish.\r\n");
	
	return (iRet != 0)?5:0;			//eeprom_read_buf failed
}

//11:		ReadOptionBufferIndex: g_szArg_netif is empty

//-1: v400_boardres_ind2usbpath:failed
//7: 	v400_boardres_ind2netif:Get szNetifPath failed
//8:		v400_boardres_ind2netif:read interface name to szNetifName failed
//9:		v400_boardres_ind2netif:szNetifName is empty
//10:  	v400_boardres_ind2netif:failed

//11:		CreateModuleSocketClient:no network interface specified
//12:		CreateModuleSocketClient:network interface * doesn't exist
//13:		CreateModuleSocketClient:Create socket failed
//14:		CreateModuleSocketClient:Set socketopt failed
//-1:		CreateModuleSocketClient:failed

//15:	_netclient_cmd_ReadEEPRom:Buffer is null
//16:	_netclient_cmd_opt:send cmd(%s) failed!
//17:	_netclient_cmd_opt:recv cmd(%s)  netif(%s) failed!
//18:	_netclient_cmd_opt:read cmd(%s) options failed!
//20:	_netclient_cmd_opt:cmd invalid
//end
int ReadOptionBufferIndex(unsigned char *buff, int start,int iLen, int index)
{
	DebugPrintf("in ReadOptionBufferIndex: index = %d \n",index);

	int iRet = v400_boardres_ind2netif(index,g_szArg_netif);

	if(0 != iRet)
	{
		printf("ReadOptionBufferIndex:  Get g_szArg_netif failed.ErrorCode = %d.\n",iRet);
		return iRet;
	}

	if('\0' == g_szArg_netif[0])
	{
		printf("ReadOptionBufferIndex: g_szArg_netif is empty.\n");
		return 11;
	}
	
	int skClient = -1;
	iRet = CreateModuleSocketClient(&skClient);
	
	if(0 != iRet)
	{
		printf("ReadOptionBufferIndex: CreateModuleSocketClient,t skClient failed.ErrorCode = %d.\n",iRet);
		return iRet;
	}
	
	//"opthex"
	char pBuff[SIZE_EEPRom] = {'\0'};
	iRet = _netclient_cmd_ReadEEPRom(skClient,pBuff);
	
	if (skClient > 0)
	{
		close(skClient);
	}
	
	//去掉中间的空格及其它字符
	int j = 0;
	for(int i = 0; i < strlen((char *)pBuff); i++)
	{
		//十六进制数,0到9,A到F
		if((('0' <= pBuff[i]) && (pBuff[i] <= '9')) || (('a' <= pBuff[i]) && (pBuff[i] <= 'f')) || (('A' <= pBuff[i]) && (pBuff[i] <= 'F')))
		{
			pBuff[j++] = pBuff[i];
		}
	}
	pBuff[j] = '\0';	
	//DebugPrintf("in ReadOptionBuffer: 去掉中间的空格及其它字符 .\n j = %d,pBuff = %s \n",j,pBuff);
	
	//将十六进制字符串转为byte字节数组	
	UCHAR temp_data[EEPROM_MAXSIZE] = {'\0'};
	char temp_small[3] = {'\0'};
	int k = 0;
	for(int i = 0; i < ((j <= EEPROM_MAXSIZE * 2) ? j : EEPROM_MAXSIZE * 2); i += 2)
	{
		memset(temp_small,0,3);
		memcpy(temp_small,pBuff + i,2);
		int iValue = 0;
		sscanf(temp_small,"%x",&iValue);
		temp_data[k++] = (UCHAR)iValue;
	}
	//DebugPrintfArray("\nReadOptionBufferIndex temp_data", temp_data, k);

	if(start + iLen <= EEPROM_MAXSIZE)
	{
		memset(buff,0,EEPROM_MAXSIZE);
		memcpy(buff,temp_data + start,iLen);
	}

	//DebugPrintf("nReadOptionBufferIndex: start = %d,iLen = %d,k = %d,iRet = %d.\n",start,iLen,k,iRet);
	DebugPrintfArray("\nReadOptionBufferIndex Finish", buff, iLen);
	
	return iRet;
}

unsigned short ft2232_checksum(unsigned char * eeprom, int size)
{
	unsigned short checksum, value;
	int i;
	
	checksum = 0xAAAA;
	
	for(i = 0; i < size/2-1; i ++){
		value = eeprom[i*2];
		value += eeprom[(i*2)+1] << 8;
		
		checksum = value^checksum;
		checksum = (checksum << 1)|(checksum >> 15);
	}
	
	return checksum;
}

//-1:		read_options:failed
//end
int read_options(unsigned char * options)
{
	int i;
	int ret;
	unsigned short checksum;
	unsigned short oldchecksum;

	if ((ret = ftdi_read_eeprom(&ux400_ftdic, eeprom)) < 0)
	{
		fprintf(stderr, "ftdi_write_eeprom failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
		return EXIT_FAILURE;
	}

	checksum = ft2232_checksum(eeprom, EEPROM_SIZE);
	oldchecksum = (eeprom[EEPROM_SIZE-1] << 8) | eeprom[EEPROM_SIZE-2];
	
	if(checksum != oldchecksum){
		printf("Checksum error, old: 0x%x, new: 0x%x\n", oldchecksum, checksum);
		return -1;
	}

	for(i = 0; i < OPTION_SIZE_FTDI; i ++){
		if(options != NULL) options[i] = eeprom[OPTION_OFFSET_FTDI+i];
		else	return -1;
	}
	
	return 0;
}

//EXIT_FAILURE: write_options:failed
//end
int write_options(unsigned char * options)
{
	int i;
	int ret;
	unsigned short checksum;
	unsigned short oldchecksum;

	if(options == NULL) return -1;
	
	if ((ret = ftdi_read_eeprom(&ux400_ftdic, eeprom)) < 0)
	{
		fprintf(stderr, "ftdi_read_eeprom failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
		return EXIT_FAILURE;
	}

	checksum = ft2232_checksum(eeprom, EEPROM_SIZE);
	oldchecksum = (eeprom[EEPROM_SIZE-2] << 8) | eeprom[EEPROM_SIZE-1];

	for(i = 0; i < OPTION_SIZE_FTDI; i ++){
		eeprom[OPTION_OFFSET_FTDI+i] = options[i];
	}

	checksum = ft2232_checksum(eeprom, EEPROM_SIZE);
	printf("The checksum: 0x%x\n", checksum);

	eeprom[EEPROM_SIZE-2] = checksum;
	eeprom[EEPROM_SIZE-1] = checksum >> 8;

#if 0
	if ((ret = ftdi_write_eeprom_bytes(&ux400_ftdic, eeprom+OPTION_OFFSET_FTDI, OPTION_SIZE_FTDI)) < 0)
	{
		fprintf(stderr, "ftdi_write_eeprom_bytes failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
		return EXIT_FAILURE;
	}

	if ((ret = ftdi_write_eeprom_bytes(&ux400_ftdic, eeprom+EEPROM_SIZE-2, 2)) < 0)
	{
		fprintf(stderr, "ftdi_write_eeprom_bytes failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
		return EXIT_FAILURE;
	}
#endif

	if ((ret = ftdi_write_eeprom(&ux400_ftdic, eeprom)) < 0)
	{
		fprintf(stderr, "ftdi_write_eeprom failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
		return EXIT_FAILURE;
	}

	return 0;
}

//4: 	sys_init:failed
//end
int sys_init(unsigned char * sn)
{
	int ret;
	
	if (ftdi_init(&ux400_ftdic) < 0)
	{
		printf("sys_init: ftdi_init failed\n");
		return 4;
	}

	if((ret = ftdi_usb_open_desc(&ux400_ftdic, UX400VENDOR, UX400PRODUCT, NULL, (char *)sn)) < 0)
	{
		printf("sys_init: ftdi_usb_open_desc failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
		return 4;
	}
	
	return 0;
}

//-1:	read_options:failed
//1:		ReadOptionBufferIndex_40g: Parameter invalid
//4: 	sys_init:failed
//end
int ReadOptionBufferIndex_40g(unsigned char *buff, int start,int iLen, int index)
{
	DebugPrintf("in ReadOptionBufferIndex_40g: index = %d \n",index);

	int iRet = 0;

	if(start + iLen > OPTION_SIZE_FTDI)
	{
		return 1;
	}

	char sn[256] = {'\0'};
	iRet = v400_boardres_ind2sn(index,sn);
	if(0 == iRet)
	{
		DebugPrintf("ReadOptionBufferIndex_40g: sn = %s\n",sn);
		
		int i = 0;
		unsigned char temp[OPTION_SIZE_FTDI];

		//memcpy(buff,TEST + start,iLen);		
		//DebugPrintfArray("ReadOptionBufferIndex_40g:  ", TEST, iLen);
		//return 0;
		//memcpy(sn,testSN,strlen(testSN));

		iRet = sys_init((UCHAR *)sn);
		if( iRet != 0 ){
			printf("ReadOptionBufferIndex_40g: FTDI init failed for serial number: %s\n", sn);
			return iRet;
		}

		iRet = read_options( temp );
		if( iRet != 0 ){
			printf("ReadOptionBufferIndex_40g: FTDI read options failed for serial number: %s\n", sn);
			ftdi_deinit(&ux400_ftdic);
			return iRet;
		}

		DebugPrintfArray("OPTION_READ_40G", temp, OPTION_SIZE_FTDI);

		memcpy(buff,temp + start,iLen);
		ftdi_deinit(&ux400_ftdic);
	}
	else
	{
		printf("ReadOptionBufferIndex_40g:v400_boardres_ind2sn failed.");
	}

	return iRet;
}

//3:		WriteOptionBuffer:Len is invalid
//4:		WriteOptionBuffer:eprom_open failed
//4:		WriteOptionBuffer:eeprom_write_buf failed
//end
//pBuff:写入的内容,
//start:开始写入的位置
//length:写入的长度
int WriteOptionBuffer(unsigned char *pBuff, int start,int iLen)
{
	DebugPrintf("in WriteOptionBuffer: No Index .start = %d,ilen = %d,pBuff = %s.\n",start,iLen,pBuff);
	int iRet = 0;
	BYTE bFlag = 0;
	
	if((iLen > E2PROM_OPTION_SIZE) || (iLen <= 0))
	{
		printf("WriteOptionBuffer: Len is invalid.\n");
		return 3;
	}
	
	iRet = eeprom_open();
	if( -1 == iRet)
	{
		printf("WriteOptionBuffer: eprom_open failed.\n");
		return 4;
	}
	
	iRet = eeprom_write_buf(start, iLen, pBuff);	
	eeprom_close();

	DebugPrintf("WriteOptionBuffer: No Index Finish.iRet = %d.\n",iRet);
	return (0 != iRet)?19:0;	//eeprom_write_buf failed
}

//11:	WriteOptionBufferIndex:g_szArg_netif is empty

//-1: v400_boardres_ind2usbpath:failed
//7: 	v400_boardres_ind2netif:Get szNetifPath failed
//8:		v400_boardres_ind2netif:read interface name to szNetifName failed
//9:		v400_boardres_ind2netif:szNetifName is empty
//10:  	v400_boardres_ind2netif:failed

//11:		CreateModuleSocketClient:no network interface specified
//12:		CreateModuleSocketClient:network interface * doesn't exist
//13:		CreateModuleSocketClient:Create socket failed
//14:		CreateModuleSocketClient:Set socketopt failed
//-1:		CreateModuleSocketClient:failed

//15:	_netclient_cmd_WriteEEPRom:Buffer is NULL
//15:	_netclient_cmd_opt:Buffer is NULL
//20:	_netclient_cmd_opt:cmd invalid
//16:	_netclient_cmd_opt:send cmd(%s) failed!
//17:	_netclient_cmd_opt:recv cmd(%s)  netif(%s) failed!
//18:	_netclient_cmd_opt:read cmd(%s) options failed!
//end
int WriteOptionBufferIndex(unsigned char *pBuff, int start,int iLen,int index)
{
	DebugPrintf("in WriteOptionBufferIndex: index = %d \n",index);

	int iRet = v400_boardres_ind2netif(index,g_szArg_netif);

	if(0 != iRet)
	{
		printf("WriteOptionBufferIndex: v400_boardres_ind2netif, Get g_szArg_netif failed.ErrorCode = %d.\n",iRet);
		return iRet;
	}

	if('\0' == g_szArg_netif[0])
	{
		printf("WriteOptionBufferIndex: g_szArg_netif is empty.\n");
		return 11;
	}
	
	int skClient = -1;
	iRet = CreateModuleSocketClient(&skClient);
	
	if(0 != iRet)
	{
		printf("WriteOptionBufferIndex: CreateModuleSocketClient, Get skClient failed.ErrorCode = %d.\n",iRet);
		return iRet;
	}

	//optfill --begin 地址 数据
	iRet = _netclient_cmd_WriteEEPRom(skClient,start,iLen,pBuff);

	if (skClient > 0)
	{
		close(skClient);
	}

	return iRet;
}

//1:		WriteOptionBufferIndex_40g: Parameter invalid

//-1:	v400_boardres_ind2sn:failed
//11:	v400_boardres_ind2sn:check parameters  failed
//21:	v400_boardres_ind2sn:check whether device exist failed
//end

int WriteOptionBufferIndex_40g(unsigned char *pBuff, int start,int iLen,int index)
{
	DebugPrintf("in WriteOptionBufferIndex_40g: start = %d,iLen = %d, index = %d \n",start,iLen,index);
	
	int iRet = 0;

	if(start + iLen > OPTION_SIZE_FTDI)
	{
		return 1;
	}

	char sn[256] = {'\0'};
	iRet = v400_boardres_ind2sn(index,sn);
	if(0 == iRet)
	{
		DebugPrintf("WriteOptionBufferIndex_40g: sn = %s\n",sn);
		
		int i = 0;
		unsigned char temp[OPTION_SIZE_FTDI];
		memset(temp,0,OPTION_SIZE_FTDI);
		//test
		//memcpy(TEST + start,pBuff,iLen);
		//DebugPrintfArray("WriteOptionBufferIndex_40g:  ", TEST, OPTION_SIZE_FTDI);
		//return 0;
		//memcpy(sn,testSN,strlen(testSN));
		//DebugPrintf("WriteOptionBufferIndex_40g: sn = %s,testSn = %s.\n",sn,testSN);
		
		iRet = sys_init((UCHAR *)sn);
		if( iRet != 0 ){
			printf("WriteOptionBufferIndex_40g: FTDI init failed for serial number: %s\n", sn);
			return iRet;
		}

		if((0 != start) || (OPTION_SIZE_FTDI != iLen))
		{
			iRet = read_options( temp );
			if( iRet != 0 )
			{
				printf("WriteOptionBufferIndex_40g: FTDI read options failed for serial number: %s\n", sn);
				ftdi_deinit(&ux400_ftdic);
				return iRet;
			}
		}

		DebugPrintfArray("OPTION_WRITE_40G_start", temp, OPTION_SIZE_FTDI);
		memcpy(temp + start,pBuff,iLen);
		//temp[0] = '7';
		//temp[1] = 0;
		//temp[2] = '8';
		//temp[3] = 0;

		iRet = write_options( temp );
		if( iRet != 0 ){
			printf("WriteOptionBufferIndex_40g: FTDI write options failed for serial number: %s\n", sn);
			ftdi_deinit(&ux400_ftdic);
			return iRet;
		}
		
		DebugPrintfArray("OPTION_WRITE_40G", temp, OPTION_SIZE_FTDI);

		ftdi_deinit(&ux400_ftdic);
	}
	else
	{
		printf("ReadOptionBufferIndex_40g:v400_boardres_ind2sn failed.");
	}

	return iRet;
}

//  0;					//Success
//  1;					//Parameter invalid
//  2;					//Malloc failure	
//  3;					//Length invalid
//  4;					//eeprom_open failed
//  5;					//eeprom_read_buf failed
//  6;					//ind2usbpath failed
//  7;					//read the path to szNetifPath failed
//  8;		  				//opendir szNetifPath failed
//  9;					//read interface name to szNetifName failed
//  10;					//fill returning values failed
//  11;					//g_szArg_netif is empty or NULL
//  12;					//network interface doesn't exist!
//  13;					//create socket client failed
//  14;					//setsockopt socket exception
//  15;					//Buffer is NULL
//  16;					//send "opthex" failed!
//  17;					//recv "opthex"  failed!
//  18;					//read \"opthex\" options failed!
//  19;					//eeprom_write_buf failed
//  20;					//send \"optfill\" failed!
//  21;					//recv \"optfill\" failed!
//  22;					//return result error

int ReadModuleBuf(PMODULEINFO pModuleInfo,unsigned char *pBuff)
{
	DebugPrintf("in ReadModuleBuf\n");
	if(NULL == pBuff)
	{
		return 2;	//Malloc failure	
	}

	if(IndexValid(pModuleInfo->iIndex))
	{
		int iRet = 0;
		int iType = GetBoardresType(pModuleInfo->iIndex);
		
		if((iType == eV400_board_40g) || (iType == eV400_board_2g5))
		{
			iRet = ReadOptionBufferIndex_40g(pBuff,pModuleInfo->iStartAddr,pModuleInfo->iLength,pModuleInfo->iIndex);
		}
		else
		{
			iRet = ReadOptionBufferIndex(pBuff,pModuleInfo->iStartAddr,pModuleInfo->iLength,pModuleInfo->iIndex);
		}
		//有索引的情况
		return iRet;
	}
	else
	{
		//没有索引的情况
		return ReadOptionBuffer(pBuff,pModuleInfo->iStartAddr,pModuleInfo->iLength);
	}
}

//-1:	v400_boardres_ind2sn:failed | v400_boardres_ind2usbpath:failed | CreateModuleSocketClient:failed
//0:		WriteModuleBuf:Success
//1:		WriteOptionBufferIndex_40g: Parameter invalid
//2:		WriteModuleBuf:Malloc failure
//3:		WriteOptionBuffer:Len is invalid
//4:		WriteOptionBuffer:eprom_open failed | WriteOptionBuffer:eeprom_write_buf failed

//7: 	v400_boardres_ind2netif:Get szNetifPath failed
//8:		v400_boardres_ind2netif:read interface name to szNetifName failed
//9:		v400_boardres_ind2netif:szNetifName is empty
//10:  	v400_boardres_ind2netif:failed
//11:	v400_boardres_ind2sn:check parameters  failed | WriteOptionBufferIndex:g_szArg_netif is empty | CreateModuleSocketClient:no network interface specified
//12:	CreateModuleSocketClient:network interface * doesn't exist
//13:	CreateModuleSocketClient:Create socket failed
//14:	CreateModuleSocketClient:Set socketopt failed	
//15:	_netclient_cmd_opt:Buffer is NULL | _netclient_cmd_WriteEEPRom:Buffer is NULL
//16:	_netclient_cmd_opt:send cmd(%s) failed!
//17:	_netclient_cmd_opt:recv cmd(%s)  netif(%s) failed!
//18:	_netclient_cmd_opt:read cmd(%s) options failed!

//20:	_netclient_cmd_opt:cmd invalid
//21:	v400_boardres_ind2sn:check whether device exist failed

//end
int WriteModuleBuf(PMODULEINFO pModuleInfo,unsigned char *pBuff)
{
	DebugPrintf("in WriteModuleBuf\n");
	if(NULL == pBuff)
	{
		return 2;	//Malloc failure	
	}

	if(IndexValid(pModuleInfo->iIndex))
	{
		int iRet = 0;
		
		int iType = GetBoardresType(pModuleInfo->iIndex);
		
		if((iType == eV400_board_40g) || (iType == eV400_board_2g5))
		{
			iRet = WriteOptionBufferIndex_40g(pBuff,pModuleInfo->iStartAddr,pModuleInfo->iLength,pModuleInfo->iIndex);
		}
		else
		{
			iRet = WriteOptionBufferIndex(pBuff,pModuleInfo->iStartAddr,pModuleInfo->iLength,pModuleInfo->iIndex);
		}
		//有索引的情况
		return iRet;
	}
	else
	{
		//没有索引的情况
		return WriteOptionBuffer(pBuff,pModuleInfo->iStartAddr,pModuleInfo->iLength);
	}
}

//读取MAC地址
int ReadMAC(unsigned char *pMac)
{
	int  iRet   = 0;
	if(NULL == pMac)
	{
			printf("ReadMAC: Mac = NULL\n");
		return NULL_POINTER_ERR;
	}
	
	memset(pMac,0,6);

	iRet = ReadOptionBuffer(pMac,0,6);
	
		return iRet;
}

//读取序列号
int ReadSerialNumber(unsigned char *pSerialNumber)
{
	int  iRet   = 0;
	if(NULL == pSerialNumber)
	{
		printf("ReadSerialNumber:pSerialNumber = NULL\n");
		return NULL_POINTER_ERR;
	}
	
	memset(pSerialNumber,0,14);
	
	iRet = ReadOptionBuffer(pSerialNumber,16,14);
	
		return iRet;
}

int WriteMAC(unsigned char *pMac)
{
	DebugPrintf("WriteMAC:%s.\n",pMac);
	return WriteOptionBuffer(pMac,0,6);
}

int WriteSerialNumber(unsigned char *pSerNo)
{
	DebugPrintf("WriteSerialNumber:%s.\n",pSerNo);
	return WriteOptionBuffer(pSerNo,16,14);
}

/******************************************************************************
   Function : CalAscII
   Purpose  : Check if a password is right or not.
   Input	: stOptTbl - the flash option serial code and user password.
						   
   Output   : none
   Return   : true - password is correct.
			  false- password is incorrect.
   Reference: CalAscII
   Called by: none

   1.Author	  : Roman
	 Create Date : 2006-11-13
	 History	 : Create function
  2,2012-06-15,jiang
  index : 索引,
******************************************************************************/
 bool CheckPwd(OPTTABLEINFO300 stOptTbl,UCHAR mac[],UCHAR serAll[])
{	
	int iMacLen = 6;			//sizeof(mac)
	int iSnLen = strlen((char *)serAll);			//sizeof(serAll)	//序列号当作字符串处理	//带索引的序列号长度为15,不带索引的长度为14,

	DebugPrintf("in CheckPwd:iMacLen = %d,iSnLen = %d.\n",iMacLen,iSnLen);
	DebugPrintfArray("CheckPwd.MAC", mac, iMacLen);
	DebugPrintfArray("CheckPwd.SN", serAll, iSnLen);
	
	UCHAR* temp = new UCHAR[stOptTbl.offset + 6 + 10 + iSnLen];
	UCHAR* md5checksum;
	UCHAR password[9];
	
	memcpy(temp, mac, iMacLen);
	memset(temp + iMacLen, stOptTbl.offset % 62, stOptTbl.offset + 10);
	memcpy(temp + iMacLen+ stOptTbl.offset + 10, serAll, iSnLen);

	md5checksum = MD5Data(temp, stOptTbl.offset + 6 + 10 + iSnLen, md5checksum);
	
	for(int i = 0; i < 8; i++)
	{
		int tmp = (int)md5checksum[2 * i + 1] * 256 + (int)md5checksum[2 * i];
		tmp = tmp % 62;

		if (tmp < 26)
			  {
					password[i] = (UCHAR)(65 + tmp);
			  }
			  else if(tmp < 36)
			  {
					password[i] = (UCHAR)(22 + tmp);
			  }
			  else
			  {
					password[i] = (UCHAR)(61 + tmp);
			  }
	}
	
	DebugPrintf("CheckPwd:calc pwd:%s, get from client pwd: %s, offset is:%d\n", password, stOptTbl.szPwd, stOptTbl.offset);

	delete temp;
	delete md5checksum;
	
	if(memcmp(password, stOptTbl.szPwd, 8))
	{
		return false;
	}
	
	return true;
}

int BitOptionDeal(int iSource,int iBit,int iData)
{
	if(0 == iData)
	{
		switch(iBit)
		{
			case 0:
				{
					iSource &= 0xFE;
				}
				break;
			case 1:
				{
					iSource &= 0xFD;
				}
				break;
			case 2:
				{
					iSource &= 0xFB;
				}
				break;
			case 3:
				{
					iSource &= 0xF7;
				}
				break;
			case 4:
				{
					iSource &= 0xEF;
				}
				break;
			case 5:
				{
					iSource &= 0xDF;
				}
				break;
			case 6:
				{
					iSource &= 0xBF;
				}
				break;
			case 7:
				{
					iSource &= 0x7F;
				}
				break;
			default:
					break;
		
		}
	
	}
	else if(1 == iData)
	{
		switch(iBit)
		{
			case 0:
				{
					iSource |= 0x01;
				}
				break;
			case 1:
				{
					iSource |= 0x02;
				}
				break;
			case 2:
				{
					iSource |= 0x04;
				}
				break;
			case 3:
				{
					iSource |= 0x08;
				}
				break;
			case 4:
				{
					iSource |= 0x10;
				}
				break;
			case 5:
				{
					iSource |= 0x20;
				}
				break;
			case 6:
				{
					iSource |= 0x40;
				}
				break;
			case 7:
				{
					iSource |= 0x80;
				}
				break;
			default:
					break;
		
		}
	
	}

	return iSource;
}

//-1:	v400_boardres_ind2sn:failed | read_options:failed | v400_boardres_ind2sn:failed | v400_boardres_ind2usbpath:failed | CreateModuleSocketClient:failed 
//1: 	WriteOptionBit: Parameter invalid | ReadOptionBufferIndex_40g: Parameter invalid | WriteOptionBufferIndex_40g: Parameter invalid

//3:		WriteOptionBuffer:Len is invalid
//4:		WriteOptionBuffer:eprom_open failed | WriteOptionBuffer:eeprom_write_buf failed | sys_init:failed

//7: 	v400_boardres_ind2netif:Get szNetifPath failed
//8:		v400_boardres_ind2netif:read interface name to szNetifName failed
//9:		v400_boardres_ind2netif:szNetifName is empty
//10:  	v400_boardres_ind2netif:failed
//11:	CreateModuleSocketClient:no network interface specified | v400_boardres_ind2sn:check parameters  failed | ReadOptionBufferIndex: g_szArg_netif is empty | WriteOptionBufferIndex:g_szArg_netif is empty
//12:	CreateModuleSocketClient:network interface * doesn't exist
//13:	CreateModuleSocketClient:Create socket failed
//14:	CreateModuleSocketClient:Set socketopt failed
//15:	_netclient_cmd_ReadEEPRom:Buffer is null
//16:	_netclient_cmd_opt:send cmd(%s) failed!
//17:	_netclient_cmd_opt:recv cmd(%s)  netif(%s) failed!
//18:	_netclient_cmd_opt:read cmd(%s) options failed!

//20:	_netclient_cmd_opt:cmd invalid
//21:	WriteOptionBit:  CheckPwd failed | v400_boardres_ind2sn:check whether device exist failed

//end
int WriteOptionBit(POPTIONINFO pOptionInfo)
{
	DebugPrintf("in WriteOptionBit....\n");
	
	int iRet = 0;	
	UCHAR mac[6] = {'\0'};
	UCHAR serAll[16] = {'\0'};
	memset(mac,0,6);
	memset(serAll,0,16);
	int iCount = pOptionInfo->optTableInfo.offset / 8 + EEPRomStart;
	int iBit = pOptionInfo->optTableInfo.offset % 8;
	if((iCount < EEPRomStart) || (iCount >= EEPROM_MAXSIZE))
	{
		printf("WriteOptionBit: Parameter invalid,iCount[%d - %d] = %d.\n",EEPRomStart,EEPROM_MAXSIZE,iCount);
		return 1;
	}
	
	if(IndexValid(pOptionInfo->index))
	{
		int iType = GetBoardresType(pOptionInfo->index);
		
		if((iType == eV400_board_40g) || (iType == eV400_board_2g5))
		{
			iCount = pOptionInfo->optTableInfo.offset / 8;
			if((iCount < 0) || (iCount >= OPTION_SIZE_FTDI))
			{
				printf("WriteOptionBit: Parameter invalid,iCount[%d - %d] = %d.\n",0,OPTION_SIZE_FTDI,iCount);
				return 1;
			}
			
			//带索引
			char sn[256] = {'\0'};
			iRet = v400_boardres_ind2sn(pOptionInfo->index,sn);
			if(0 != iRet)
			{
				return iRet;
			}
			else
			{
				memcpy(serAll,sn,15);
			}

			for(int j = 0; j < 6; j++)
			{
			 	mac[j] = (UCHAR)(0);
			}

			if(CheckPwd(pOptionInfo->optTableInfo, mac, serAll))
			{
				unsigned char pBuff[OPTION_SIZE_FTDI];
				memset(pBuff,0,OPTION_SIZE_FTDI);
				iRet = ReadOptionBufferIndex_40g(pBuff, 0, OPTION_SIZE_FTDI, pOptionInfo->index);
				
				if(0 != iRet)
				{
					return iRet;
				}
				
				UCHAR tempData[1] = {0};
				int nValue = (int)pBuff[iCount];
				nValue = BitOptionDeal(nValue, iBit, pOptionInfo->data);
				tempData[0] = (UCHAR)nValue;

				iRet = WriteOptionBufferIndex_40g(tempData, iCount,1, pOptionInfo->index);					
			}
			else
			{
				iRet = 21;
				printf("WriteOptionBit:  CheckPwd failed.offset = %d, pass = %s,data = %d,iIndex = %d \n",pOptionInfo->optTableInfo.offset,pOptionInfo->optTableInfo.szPwd,pOptionInfo->data,pOptionInfo->index);
			}
		}		
		else
		{
			//带索引
			unsigned char pBuff[EEPROM_MAXSIZE] = {'\0'};
			iRet = ReadOptionBufferIndex(pBuff,0,EEPROM_MAXSIZE, pOptionInfo->index);
			if(0 != iRet)
			{
				return iRet;
			}
			
			memcpy(mac,pBuff,6);
			memcpy(serAll,pBuff + 32,15);

			if(CheckPwd(pOptionInfo->optTableInfo, mac, serAll))
			{
				UCHAR tempData[1] = {0};
				int nValue = (int)pBuff[iCount];
				nValue = BitOptionDeal(nValue, iBit, pOptionInfo->data);
				tempData[0] = (UCHAR)nValue;

				iRet = WriteOptionBufferIndex(tempData, iCount,1, pOptionInfo->index);
				
				//memcpy(tempData,pBuff + iCount,1);
				//sscanf((char *)tempData,"%x",&nValue);
				//char *str;
				//long i = strtol((char *)tempData,&str,16);
				//DebugPrintf("WriteOptionBit2:nValue = %d,i = %d.\n",nValue,i);
				
				//DebugPrintf("WriteOptionBit3:BitOptionDeal.nValue = %d.\n",nValue);
				//memset(tempData,0,3);
				//sprintf((char *)tempData,"%02X",nValue);
				//tempData[2] = '\0';
				//DebugPrintf("WriteOptionBit4:tempData = %s.\n",tempData);			
			}
			else
			{
				iRet = 21;
				printf("WriteOptionBit:  CheckPwd failed.offset = %d, pass = %s,data = %d,iIndex = %d \n",pOptionInfo->optTableInfo.offset,pOptionInfo->optTableInfo.szPwd,pOptionInfo->data,pOptionInfo->index);
			}
		}
	}
	else
	{
		UCHAR pBuff[EEPROM_MAXSIZE] = {'\0'};
		iRet = ReadOptionBuffer(pBuff, 0, EEPROM_MAXSIZE);
		if(0 != iRet)
		{
			return iRet;
		}

		memcpy(mac,pBuff,6);
		memcpy(serAll,pBuff + 16,14);

		if(CheckPwd(pOptionInfo->optTableInfo, mac, serAll))
		{
			int nValue = (int)(pBuff[iCount]);
			//DebugPrintf("WriteOptionBit:No1,nValue = %d.\n",nValue);
			//sscanf((char *)pBuff,"%d",&nValue);
			nValue = BitOptionDeal(nValue, iBit, pOptionInfo->data);
			//DebugPrintf("WriteOptionBit2:No1,nValue = %d.\n",nValue);			
			pBuff[iCount] = (UCHAR)nValue;
			DebugPrintfArray("Write Buff", pBuff, EEPROM_MAXSIZE);
			
			iRet = WriteOptionBuffer(pBuff, 0, EEPROM_MAXSIZE);
		}
		else
		{
			iRet = 21;
			printf("WriteOptionBit:  CheckPwd failed.offset = %d, pass = %s,data = %d,iIndex = %d \n",pOptionInfo->optTableInfo.offset,pOptionInfo->optTableInfo.szPwd,pOptionInfo->data,pOptionInfo->index);
		}
	}

	DebugPrintf("WriteOptionBit: Finish,iRet = %d.\n",iRet);
	return iRet;
}

/******************************************************************************
   Function : WriteMemory
   Purpose  : Write data to the physical memory address, and report the result
   Input	: pstMemData - get the physical address, length and data to write
   Output   : none
   Return   : 0 - success
			  others - fail
   Reference: GetVirtualMemory
   Called   : DealTcpSvrData

   1.Author	  : cjiang
	 Create Date : 2012-05-23
	 History	 : Create function
******************************************************************************/
int WriteMemory(MEMDATA *pstMemData)
{
	ULONG ulAddr = 0;
	ULONG ulLen  = 0;
	ULONG ulCount= 0;
	UCHAR *pVirAdd = NULL;	
	
	if( NULL == pstMemData )
	{
		return NULL_POINTER_ERR;
	}

	ulAddr = pstMemData->stMemInfo.ulStartAddr;
	ulLen  = pstMemData->stMemInfo.ulLength;
	//ulAddr = ntohl(pstMemData->stMemInfo.ulStartAddr);
	//ulLen  = ntohl(pstMemData->stMemInfo.ulLength);
	
	if( 0 == ulLen || ulLen > MAX_MEM_LEN )
	{
		return INVALID_ARG_ERR;
	}

	if( MIN_PHY_ADDR > ulAddr )
	{
		return INVALID_PHY_ADD;
	}

	DebugPrintf("WriteMemory: yAddr:[0x%x], length:%d\n", ulAddr, ulLen);
	
	pVirAdd = (UCHAR *)(pstMemData->stMemInfo.ulStartAddr);
	//pVirAdd = (UCHAR *)GetVirtualMemory( (void *)ulAddr, NULL );
	//bugPrintf("VirtAddr:[0x%x], length:%d\n", pVirAdd, ulLen);
   
	//bugPrintf("Writing into memory...\n");
	for( ulCount = 0; ulCount < ulLen; ulCount++ )
	{
		*pVirAdd = pstMemData->pucBuf[ulCount];
		//bugPrintf("0x%03x:0x%02x,0x%02x ", ulCount, pstMemData->pucBuf[ulCount], *pVirAdd);
			if( 0 == (ulCount +1) % 10 )
			{
				//bugPrintf("\n");
			}
		pVirAdd++;
	}

	DebugPrintf("WriteMemory: Writing into memory finished.\n");   
	sleep(1);

	return 0;
}

/******************************************************************************
   Function : ReadMemory
   Purpose  : Read data from the physical memory address, and report the result
   Input	: stMemInfo - get the physical memory address and length to read
   Output   : pstMemData
   Return   : 0 - success
			  others - fail
   Reference: GetVirtualMemory
   Called   : DealTcpSvrData
   
   1.Author	  : cjiang
	 Create Date : 2012-05-23
	 History	 : Create function
******************************************************************************/
int ReadMemory(MEMDATA *pstMemData)
{
	UCHAR *pVirAdd = NULL;
	ULONG ulCount = 0;
	extern bool g_IsDbg;

	if(NULL == pstMemData)
	{
		return NULL_POINTER_ERR;
	}

	//pstMemData->stMemInfo.ulLength = ntohl(pstMemData->stMemInfo.ulLength);
 //   pstMemData->stMemInfo.ulStartAddr = ntohl(pstMemData->stMemInfo.ulStartAddr);
	DebugPrintf("ReadMemory: %d, addr:0x%x\n", pstMemData->stMemInfo.ulLength, pstMemData->stMemInfo.ulStartAddr);
	if(0 == pstMemData->stMemInfo.ulLength || pstMemData->stMemInfo.ulLength > MAX_MEM_LEN )
	{
		return INVALID_ARG_ERR;
	}

	if( MIN_PHY_ADDR > pstMemData->stMemInfo.ulStartAddr )
	{
		return INVALID_PHY_ADD;
	}

	pVirAdd = (UCHAR *)(pstMemData->stMemInfo.ulStartAddr);
	//pVirAdd = (UCHAR *)GetVirtualMemory( (void *)pstMemData->stMemInfo.ulStartAddr, NULL );
	DebugPrintf("ReadMemory: PhyAddr:[0x%x], VirtAddr:[0x%x], length:%d\n", pstMemData->stMemInfo.ulStartAddr, pVirAdd, pstMemData->stMemInfo.ulLength);
   
	DebugPrintf("ReadMemory: Reading from memory...\n");
	for( ulCount = 0; ulCount < pstMemData->stMemInfo.ulLength; ulCount++ )
	{
		pstMemData->pucBuf[ulCount] = *pVirAdd;

		if( true == g_IsDbg )
		{
			DebugPrintf("0x%03x:0x%02x ", ulCount, *pVirAdd);
			if( 0 == (ulCount +1) % 10 )
			{
				DebugPrintf("\n");
			}
		}		
		pVirAdd++;
	}
	
	DebugPrintf("\nReadMemory: Reading finished.len:%d\n", ulCount);	

	pstMemData->stMemInfo.ulLength = ulCount;
	pstMemData->stMemInfo.ulStartAddr = pstMemData->stMemInfo.ulStartAddr;
	
	return 0;
}

int FindAllSN()
{
	int ret, i;
	struct ftdi_context ftdic;
	struct ftdi_device_list *devlist, *curdev;
	char manufacturer[128], description[128], serialno[128];

	if (ftdi_init(&ftdic) < 0)
	{
		fprintf(stderr, "FindAllSN: ftdi_init failed\n");
		return 1;
	}

	if ((ret = ftdi_usb_find_all(&ftdic, &devlist, 0x0403, 0x6010)) < 0)
	{
		fprintf(stderr, "FindAllSN: ftdi_usb_find_all failed: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return 2;
	}

	printf("FindAllSN: Number of FTDI devices found: %d\n", ret);

	i = 0;
	bool bIsTest = false;
	for (curdev = devlist; curdev != NULL; i++)
	{
		printf("FindAllSN: Checking device: %d\n", i);
		if ((ret = ftdi_usb_get_strings(&ftdic, curdev->dev, manufacturer, 128, description, 128, serialno, 128)) < 0)
		{
			fprintf(stderr, "FindAllSN: ftdi_usb_get_strings failed: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
			return 3;
		}
		printf("FindAllSN: Manufacturer: %s, Description: %s, Serial number: %s\n\n", manufacturer, description, serialno);
	 if(!bIsTest)
	 {
	 	memcpy(testSN,serialno,strlen(serialno));
		bIsTest = true;
	 }
		curdev = curdev->next;
	}

	ftdi_list_free(&devlist);
	ftdi_deinit(&ftdic);

	return 0;
}

//供外部程序调用,启动监听服务
bool StartCommonServer()
{
	DebugPrintf("Into StartCommonServer...\n");
	
	StopAll = false;
	if(threadHandleBroadcastListen != 0)
	{
		DebugPrintf("StartCommonServer: readHandleBroadcastListen exit...:StartCommonServer\n");
		pthread_exit( &threadHandleBroadcastListen );
	}
 	
	if(threadHandleListen != 0)
	{
		DebugPrintf("StartCommonServer: threadHandleListen exit...:StartCommonServer\n");
		pthread_exit( &threadHandleListen );
	}

	return ((!pthread_create(&threadHandleBroadcastListen, NULL, BeginMySocketBroadcastListen, NULL)) && (!pthread_create(&threadHandleListen, NULL, BeginMySocketListen, NULL)));
}

//供外部程序调用,停止监听服务
void StopCommonServer()
{
	DebugPrintf("Into StopCommonServer...\n");
	
	StopAll = true;

	while(threadHandleListen != 0 || threadHandleBroadcastListen != 0)
	{
		sleep(1);
		DebugPrintf("StopCommonServer: waiting StopCommonServer finish....\n");
	}

	DebugPrintf("StopCommonServer: Leaving StopCommonServer\n");
}
