#include <stdio.h>
#include <signal.h>			//signal
#include <dirent.h>
#include <sys/stat.h>		//mkdir()
#include <unistd.h> 
#include <stdlib.h>
#include <errno.h>			//errno
#include <time.h>
#include "macro.h"
#include "CommonServer.h"
#include "SingleService.h"


CCommonServer::CCommonServer(void)
{
	m_bIsGetFileCancel = false;	
	m_bIsSendFileCancel = false;
	m_bIsReadMemoryCancel = false;
	m_fpSendFile = NULL;		
	m_pSendFilePath = NULL;
	m_pGetFilePath = NULL;
	m_threadDownFile = 0;	
	m_threadReadMemory = 0;
	m_threadCMD = 0;
	m_bIsCMDCancel = false;
	m_threadMemoryScript = 0;	
	m_MemoryScript = NULL;			
	m_bIsMemoryScriptCancel = false;	
	m_bIsMemoryScriptFinish = true;	
	
	pthread_mutex_init(&_ServerSend_obj, null);
}

CCommonServer::~CCommonServer(void)
{
	if(NULL != m_fpSendFile)
	{
		fclose(m_fpSendFile);
		m_fpSendFile = NULL;
	}

	free(m_pSendFilePath);
	free(m_pGetFilePath);		
	m_pSendFilePath = NULL;
	m_pGetFilePath = NULL;
	
	pthread_mutex_destroy(&_ServerSend_obj);
}

void CCommonServer::Connect()
{
	DebugPrintf("Into CCommonServer::Connect!");
	//MSG_CONNECT
	if(ClientBase::SimpleSend(MSG_CONNECT))
	{
		DebugPrintf("Send Success : MSG_CONNECT.\n"); 
	}
	else
	{
        DebugPrintf("Send failed : MSG_CONNECT.\n"); 
	}

	DebugPrintf("Into CCommonServer::Connect!\n");
}

bool CCommonServer::SendData(unsigned int type, int secondsTimeout)
{
	return SendData(type, null, 0, secondsTimeout);
}

bool CCommonServer::SendData(unsigned int type, const unsigned char * data, unsigned int length, int secondsTimeout)
{
	bool bIsSendSuccess = false;
	pthread_mutex_lock(&_ServerSend_obj);
	bIsSendSuccess = ClientBase::Send(type, data,length,secondsTimeout);
	pthread_mutex_unlock(&_ServerSend_obj);

	return bIsSendSuccess;
}

static BOOL IsFolder(char* pathname,char* filename)
{
    char   name[800];
    struct stat st;

    strcpy(name, pathname);
    strcat(name,"/");
    strcat(name,filename);

    DebugPrintf("file name%s\n", name);    

    return (stat((char*)name, &st)==0 && (st.st_mode&S_IFDIR));
}

int CreateDir(const char *sPathName) 
{
	try
	{
		char DirName[256]; 
		strcpy(DirName, sPathName); 
		int i,len = strlen(DirName); 
		if(DirName[len-1] != '/')
		{
			strcat(DirName, "/");
		}

		len = strlen(DirName); 

		for(i=1; i<len; i++) 
		{
			if(DirName[i]=='/') 
			{ 
				DirName[i] = 0; 
				if(access(DirName, NULL) != 0 ) 
				{ 
					if(mkdir(DirName, 0755) == -1) 
					{
						return -1; 
					} 
				} 

				DirName[i] = '/'; 
			} 
		} 
	}
	catch(...)
	{
		return -1; 
	}

	return 0; 
}
//�����ļ�ʱ,�̵߳��õĴ�����
void *DownFile_DoWork(void *pCCServer)
{
	DebugPrintf("in DownFile_DoWork.\n");
	pthread_detach(pthread_self());

	CCommonServer *pCCommonServer = (CCommonServer *)pCCServer;
	try
	{
		pCCommonServer->m_bIsGetFileCancel = false;
		FILE * fp = fopen(pCCommonServer->m_pGetFilePath,"r");
		if(NULL == fp ) 
		{ 
			printf("DownFile_DoWork:File:\t%s Not Found\n", pCCommonServer->m_pGetFilePath); 
			if(!pCCommonServer->SendData((unsigned int)MSG_GETFILE,MILLION)) 
			{ 
				printf("DownFile_DoWork:Send File:\t%s Failed,fp == NULL.\n", pCCommonServer->m_pGetFilePath); 
			}
		} 
		else 
		{
			char *strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
			memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);

			int file_block_length = 0; 
			
			while( (file_block_length = fread(strTemp,sizeof(char),NR_MAXPACKLEN,fp))>0)
			{
				if(pCCommonServer->m_bIsGetFileCancel)
				{
					printf("DownFile_DoWork: Get file cancel by user.\n");
					break;
				}

				if(!pCCommonServer->SendData((unsigned int)MSG_GETFILE,(unsigned char *)strTemp,file_block_length,MILLION)) 
				{ 
					printf("DownFile_DoWork:Send File:\t%s Failed\n", pCCommonServer->m_pGetFilePath); 
					
					if(!pCCommonServer->SendData((unsigned int)MSG_GETFILE,MILLION))
					{
						printf("DownFile_DoWork:Send MSG_GETFILE cancel failed.\n");
					}
					
					break; 
				}
				memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
			}
								
			fclose(fp); 
			free(strTemp);
			strTemp = NULL;
		}
	}
	catch(...)
	{
		DebugPrintf("DownFile_DoWork:Exception.\n");
	}

	pCCommonServer->m_threadDownFile = 0;
	pthread_exit(NULL); //�˳��߳�
	DebugPrintf("DownFile_DoWork:File:\t%s Transfer Finished\n",pCCommonServer->m_pGetFilePath); 
}

void *ReadMemory_DoWork(void *pCCServer)
{
	DebugPrintf("in ReadMemory_DoWork.\n");
	pthread_detach(pthread_self());
	
	CCommonServer *pCCommonServer = (CCommonServer *)pCCServer;

	try
	{
		pCCommonServer->m_bIsReadMemoryCancel = false;

		int iReadFinish = 0;
		int iCurrentLenth = 0;

		while(iReadFinish < pCCommonServer->m_memInfoRead.ulLength)
		{
			if(pCCommonServer->m_bIsReadMemoryCancel)
			{
				break;
			}
		
			iCurrentLenth = NR_MAXPACKLEN - sizeof(MEMINFO);
			if(iCurrentLenth > pCCommonServer->m_memInfoRead.ulLength - iReadFinish)
			{
				iCurrentLenth = pCCommonServer->m_memInfoRead.ulLength - iReadFinish;
			}
		
			MEMDATA *pMemData;
			pMemData->stMemInfo.ulLength = iCurrentLenth;
			pMemData->stMemInfo.ulStartAddr = pCCommonServer->m_memInfoRead.ulStartAddr + iReadFinish;
			pMemData->pucBuf = (UCHAR *)malloc(sizeof(UCHAR) * pMemData->stMemInfo.ulLength);
			memset(pMemData->pucBuf, 0, sizeof(UCHAR) * pMemData->stMemInfo.ulLength);
			
			DebugPrintf("ReadMemory_DoWork: Read addr = 0x%x,len = %d.\n",pMemData->stMemInfo.ulStartAddr,pMemData->stMemInfo.ulLength);
			int iRet = 0;
			//iRet = ReadMemory(pMemData);

			char *strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
			memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
			int iLen = 0;
			if(0 == iRet)
			{
				sprintf(strTemp,"%d%d\0",pMemData->stMemInfo.ulStartAddr,pMemData->stMemInfo.ulLength);
				iLen = strlen(strTemp);
				memcpy(strTemp + iLen,pMemData->pucBuf,pMemData->stMemInfo.ulLength);
				iLen += pMemData->stMemInfo.ulLength;
				
				if(!pCCommonServer->SendData((unsigned int)MSG_READMEMORY,(unsigned char *)strTemp,iLen,MILLION))
				{ 
					printf("ReadMemory_DoWork: Send read memory Failed\n"); 
					if(!pCCommonServer->SendData((unsigned int)MSG_READMEMORY,MILLION))
					{
						printf("ReadMemory_DoWork: Send read memory cancel Failed\n"); 
					}
					
					free( strTemp );
					strTemp = NULL;
					break; 
				}

				free( strTemp );
				strTemp = NULL;
			}
			else
			{
				printf("ReadMemory_DoWork: ReadMemory Failed.iRet = %d.\n",iRet); 
				if(!pCCommonServer->SendData((unsigned int)MSG_READMEMORY,MILLION))
				{
					printf("ReadMemory_DoWork: Send read memory cancel Failed\n"); 
				}
				
				free( strTemp );
				strTemp = NULL;
				break;
			}

		}
	}
	catch(...)
	{
		printf("ReadMemory_DoWork: expection.\n"); 
	}

	pCCommonServer->m_threadReadMemory = 0;
	DebugPrintf("ReadMemory_DoWork: Finish.\n");
}

void *CMD_DoWork(void *pCCServer)
{
	DebugPrintf("in CMD_DoWork.\n");
	pthread_detach(pthread_self());
	
	CCommonServer *pCCommonServer = (CCommonServer *)pCCServer;

	//int iTest = 0;
	//while((!pCCommonServer->m_bIsCMDCancel) && iTest < 20)
	//{
	//	sleep(1);
	//	iTest++;	
	//}
	
	try
	{
		pCCommonServer->m_bIsCMDCancel = false;
	
		DebugPrintf("CMD_DoWork:CMD:%s.\n",pCCommonServer->m_CMD);
		if(NULL != pCCommonServer->m_CMD)
		{
			int rc = 0; // ���ڽ��������ֵ
			FILE *fp;
	
			/*ִ��Ԥ���趨�����������������ı�׼���*/
			fp = popen(pCCommonServer->m_CMD, "r");
			if(NULL == fp)
			{
				printf("CMD_DoWork:popenִ��ʧ�ܣ�");
				pCCommonServer->m_threadCMD = 0;
				return NULL;
			}

			char *strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
			memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
			while(fgets(strTemp, NR_MAXPACKLEN, fp) != NULL)
			{
				if(pCCommonServer->m_bIsCMDCancel)
				{
					printf("CMD_DoWork: CMD Cancel.\n");
					break;
				}
				
				if(!pCCommonServer->SendData((unsigned int)MSG_CMD,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
				{ 
					printf("CMD_DoWork:Send CMD result Failed\n"); 
					
					if(!pCCommonServer->SendData((unsigned int)MSG_CMD,MILLION))
					{ 
						printf("CMD_DoWork:Send CMD result end Failed\n"); 
					}
					break; 
				}

				memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
			}

			free( strTemp );
			strTemp = NULL;

			/*�ȴ�����ִ����ϲ��رչܵ����ļ�ָ��*/
			rc = pclose(fp);

			if(-1 == rc)
			{
				printf("CMD_DoWork:close fp failed.\n");
			}
			else
			{
				DebugPrintf("CMD_DoWork:���%s���ӽ��̽���״̬��%d�������ֵ��%d��\r\n", pCCommonServer->m_CMD, rc, WEXITSTATUS(rc));
			}

			if(!pCCommonServer->m_bIsCMDCancel)
			{
				if(!pCCommonServer->SendData((unsigned int)MSG_CMD,MILLION))
				{ 
					printf("CMD_DoWork: Send CMD result end Failed\n"); 
				}
			}
		}
	}
	catch(...)
	{
		printf("CMD_DoWork: expection.\n"); 
	}

	pCCommonServer->m_threadCMD = 0;
	DebugPrintf("CMD_DoWork: Finish.\n");
}

//��pSourceArray�ĵ�һά��������һ��,����ԭ�����ַ�����������,�����µ��ַ���ָ��,length��pSourceArray��һά�ĳ���
LPCHAR *MagnifyArray(LPCHAR *pSourceArray,int length)
{
	try
	{
		LPCHAR *pNewArray;
		pNewArray = (LPCHAR *)malloc(length * 2 * sizeof(char *));

		for(int i = 0; i < length; i++)
		{
			pNewArray[i] = pSourceArray[i];
		}

		return pNewArray;
	}
	catch(...)
	{
		printf("MagnifyArray:expection.\n"); 
	
		return NULL;
	}
}

//���ű��ַ��������ɵ���һ���е�����,ȥ�����е�ע�͵�,����ֵΪ����������������
//LPCHAR *ScriptToLine(char *pMemoryScript, int iScriptLength, int *piLineCount)
LPCHAR *ScriptToLine(CCommonServer * pCCommonServer, int *piLineCount)
{
	DebugPrintf("in ScriptToLine.\n");

	char *pMemoryScript = pCCommonServer->m_MemoryScript;
	int iScriptLength = pCCommonServer->m_MemoryScriptLength;

	if(NULL == pMemoryScript)
	{
		printf("ScriptToLine:NULL == pMemoryScript.\n"); 
		return NULL;
	}
	
	try
	{
		int iLineLength = 200;
		LPCHAR *pLine;
		pLine = (LPCHAR *)malloc(iLineLength * sizeof(char *));
		
		if(NULL == pLine)
		{
			printf("ScriptToLine:NULL == pLine, malloc pLine failed.\n"); 
			return NULL;
		}

		int iStarFlag = 0;		//������� /*,*/���ֵĴ���,����/*��һ,����*/��һ
		int iBit = 0;
		int iLineStartBit = 0;		//ÿһ�п�ʼ��λ��
		bool bIsNewLine = true;		//����Ƿ����µ�һ��
		*piLineCount = 0;			//�Ѿ�������������������Ŀ
		while(iBit < iScriptLength)
		{
			if(pCCommonServer->m_bIsMemoryScriptCancel)
			{
				printf("ScriptToLine: Memory script cancel.\n"); 
				break;
			}
			
			if(*(pMemoryScript + iBit) != NULL)
			{
				if(('\r' == *(pMemoryScript + iBit)) || ('\n' == *(pMemoryScript + iBit)))
				{
					iBit++;
					continue;
				}

				if(0 != iStarFlag)
				{
					//����/*.......*/�������
					if(('/' == *(pMemoryScript + iBit)) && ('*' == *(pMemoryScript + iBit + 1)))
					{
						iStarFlag++;
						iBit++;				//ָ�����һλ
					}
					else if(('*' == *(pMemoryScript + iBit)) && ('/' == *(pMemoryScript + iBit + 1)))
					{
						iStarFlag--;
						iBit++;				//ָ�����һλ
					}

					bIsNewLine = true;
				}
				else
				{
					if(('/' == *(pMemoryScript + iBit)) && ('*' == *(pMemoryScript + iBit + 1)))
					{
						bIsNewLine = true;
						iStarFlag++;
						iBit++;				//ָ�����һλ
					}
					else if(('/' == *(pMemoryScript + iBit)) && ('/' == *(pMemoryScript + iBit + 1)))
					{
						bIsNewLine = true;
						//����//.....�������,ֱ�Ӳ���'\n',�����з�,��������
						for(int i = iBit + 2; i < iScriptLength; i++)
						{
							if('\n' == *(pMemoryScript + i))
							{
								iBit = i;
								break;
							}
						}
					}
					else
					{
						//�˴�����������,��';'Ϊ�������
						//�µ�һ�п�ʼ��ʱ��
						if(bIsNewLine)
						{
							iLineStartBit = iBit;
							bIsNewLine = false;
						}

						//�ж��Ƿ��ǽ������';'
						if(';' == *(pMemoryScript + iBit))
						{
							//��������
							if(iBit - iLineStartBit > 0)
							{
								char *pTempLine;
								pTempLine = (char *)malloc((iBit - iLineStartBit + 1) * sizeof(char));
								memset(pTempLine,0,iBit - iLineStartBit + 1);
								if(NULL != pTempLine)
								{
									memcpy(pTempLine,pMemoryScript + iLineStartBit,iBit - iLineStartBit);
								}

								//�ж��Ƿ񳬳���ǰָ������ĳ���,����������ָ����������
								if(*piLineCount >= iLineLength)
								{
									pLine = MagnifyArray(pLine,iLineLength);

									if(NULL != pLine)
									{
										iLineLength = iLineLength * 2;
									}
									else
									{
										printf("ScriptToLine:MagnifyArray failed.\n"); 
										return NULL;
									}
								}
								
								pLine[*piLineCount] = pTempLine;

								(*piLineCount)++;
								bIsNewLine = true;
							}
						}
					}
				}
			}
			else
			{
				printf("ScriptToLine:*(pMemoryScript + iBit) != NULL.\n"); 
				return NULL;
			}

			iBit++;
		}

		return pLine;
	}
	catch(...)
	{
		printf("ScriptToLine:expection.\n"); 
		return 0;
	}
}

LPCHAR *GetMemoryScriptCMD(char *pOneLine,int *piCMDLength)
{
	DebugPrintf("in GetMemoryScriptCMD:%s.\n",pOneLine);
	try
	{
		LPCHAR *pSmallLine;			//���3��ָ��
		pSmallLine = (LPCHAR *)malloc(3 * sizeof(LPCHAR));

		int iLineLength = strlen(pOneLine);
		int iStartBit = -1;
		*piCMDLength = 0;
		char *pTemp;
		for(int i = 0; i < iLineLength; i++)
		{
			if((' ' != *(pOneLine + i)) && (-1 == iStartBit))
			{
				//�ӷǿո���ַ���ʼ��ȡ,��ǿ�ʼλ��
				iStartBit = i;
				
				//���һ��ֻ��һ���ַ������
				if(i == iLineLength - 1)
				{
					pTemp = (char *)malloc(sizeof(char) * (1 + 1));
					memset(pTemp,0,1 + 1);
					memcpy(pTemp,(pOneLine + iStartBit),1);
				
					pSmallLine[(*piCMDLength)++] = pTemp;
				
					iStartBit = -1;		//��ʼλ�ù���
				}
			}
			else if(((' ' == *(pOneLine + i)) || (i == iLineLength - 1) || (2 == *piCMDLength)) && (-1 != iStartBit))
			{
				//(' ' == *(pOneLine + i)):�����ո����(����:E 0X1234 34,E������ǿո����)
				//(i == iLineLength - 1):�����н���(����:S 5,��5���������ĩβ����)
				//(2 == *piCMDLength):�Ѿ���ȡ������,���һ���־���һ������(����:W 0X1234 3456 234 23 23,��������Ϊд�������,�������п���Ϊ�ո�,���Բ������Կո��ж�)
				//�����ո����,��ȡ�ӿ�ʼλ�õ��˴����ַ���
				int iLen = i - iStartBit;
				if((i == iLineLength - 1) || (2 == *piCMDLength))
				{
					iLen =  iLineLength - iStartBit;
				}

				pTemp = (char *)malloc(sizeof(char) * (iLen + 1));
				memset(pTemp,0,iLen + 1);
				memcpy(pTemp,(pOneLine + iStartBit),iLen);
				
				pSmallLine[(*piCMDLength)++] = pTemp;
				
				iStartBit = -1;		//��ʼλ�ù���

				if(3 <= *piCMDLength)
				{
					break;
				}
			}
		}

		return pSmallLine;
	}
	catch(...)
	{
		printf("GetMemoryScriptCMD:expection.\n"); 
		return NULL;
	}
}

void *MemoryScript_DoWork(void *pCCServer)
{
	DebugPrintf("in MemoryScript_DoWork.\n");
	pthread_detach(pthread_self());
	
	CCommonServer *pCCommonServer = (CCommonServer *)pCCServer;

	sleep(10);
	bool bIsSuccess = false;
	try
	{
		int iLineCount;
		LPCHAR *pLine = ScriptToLine(pCCommonServer,&iLineCount);
		if(0 != iLineCount)
		{
			if(NULL != pLine)
			{
				for(int i = 0; i < iLineCount; i++)
				{
					if(pCCommonServer->m_bIsMemoryScriptCancel)
					{
						printf("MemoryScript_DoWork: Memory script cancel.\n");
						break;
					}

					int iCMDLength = 0;
					LPCHAR *pSmallLine = GetMemoryScriptCMD(pLine[i],&iCMDLength);
					if(NULL != pSmallLine)
					{
						try
						{
							if((2 == iCMDLength) || (3 == iCMDLength))
							{
								if(NULL == (*pSmallLine))
								{
									printf("MemoryScript_DoWork: NULL == (*pSmallLine).\n");
									continue;
								}
								
								if(('E' == *(*pSmallLine)) || ('e' == *(*pSmallLine)))
								{
									if((NULL != (*(pSmallLine + 1))) && (NULL != (*(pSmallLine + 2))))
									{
										MEMDATA pstMemData;
										pstMemData.stMemInfo.ulStartAddr = (ULONG)atol((*(pSmallLine + 1)));
										pstMemData.stMemInfo.ulLength = (ULONG)atol((*(pSmallLine + 2)));
										pstMemData.pucBuf = (UCHAR *)malloc(sizeof(UCHAR) * pstMemData.stMemInfo.ulLength);
										memset(pstMemData.pucBuf,0,pstMemData.stMemInfo.ulLength);
										
										DebugPrintf("MemoryScript_DoWork:E:%x,%d.\n",pstMemData.stMemInfo.ulStartAddr,pstMemData.stMemInfo.ulLength);
										//WriteMemory(&pstMemData);
									}
									else
									{
										bIsSuccess = false;
										break;
									}
								}
								else if(('W' == *(*pSmallLine)) || ('w' == *(*pSmallLine)))
								{
									if((NULL != (*(pSmallLine + 1))) && (NULL != (*(pSmallLine + 2))))
									{
										MEMDATA pstMemData;
										pstMemData.stMemInfo.ulStartAddr = (ULONG)atol((*(pSmallLine + 1)));
										pstMemData.stMemInfo.ulLength = strlen((*(pSmallLine + 2)));
										pstMemData.pucBuf = (UCHAR *)malloc(sizeof(UCHAR) * pstMemData.stMemInfo.ulLength);
										memset(pstMemData.pucBuf,0,pstMemData.stMemInfo.ulLength);
										memcpy(pstMemData.pucBuf,(*(pSmallLine + 2)),pstMemData.stMemInfo.ulLength);
										DebugPrintf("MemoryScript_DoWork:W:%x,%d,%s.\n",pstMemData.stMemInfo.ulStartAddr,pstMemData.stMemInfo.ulLength,pstMemData.pucBuf);
										//WriteMemory(&pstMemData);
									}
									else
									{
										bIsSuccess = false;
										break;
									}
								}
								else if(('S' == *(*pSmallLine)) || ('s' == *(*pSmallLine)))
								{
									if(NULL != (*(pSmallLine + 1)))
									{
										int time = atoi((*(pSmallLine + 1)));
										//sleep(time);
										DebugPrintf("MemoryScript_DoWork:S:%d.\n",time);
									}
									else
									{
										bIsSuccess = false;
										break;
									}
								}

								bIsSuccess = true;
							}
						}
						catch(...)
						{
							bIsSuccess = false;
							printf("MemoryScript_DoWork:pSmallLine failed.\n"); 
						}
					}
					else
					{
						bIsSuccess = false;
						break;
					}
				}
			}
		}
		else
		{
			printf("MemoryScript_DoWork:ScriptToLine failed.\n"); 
		}
		
		char *strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
        	memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);

		if(pCCommonServer->m_bIsMemoryScriptCancel)
		{
			memcpy(strTemp, "1", 1);			//0 - ִ�гɹ�,1 - ִ��ʧ��	
		}
		else
		{
			memcpy(strTemp, (bIsSuccess ? "0" : "1"), 1);			//0 - ִ�гɹ�,1 - ִ��ʧ��	
		}

		if(!pCCommonServer->SendData((unsigned int)MSG_MEMORYSCRIPT_FINISH,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
		{
			printf("MemoryScript_DoWork:Send memory script result failed.\n");
		}

		free( strTemp );
		strTemp = NULL;
	}
	catch(...)
	{
		printf("MemoryScript_DoWork:expection.\n"); 
	}

	pCCommonServer->m_bIsMemoryScriptFinish = true;
	pCCommonServer->m_threadMemoryScript = 0;
	
	DebugPrintf("MemoryScript_DoWork: Finish.\n");
}

void GetPathFileInfo(char *data,char *type,char *strTemp)
{
}

void CCommonServer::OnReceived(unsigned int type, const unsigned char* data, unsigned int length)
{
    DIR *dir = NULL;
    char *strTemp = NULL;
	struct dirent *ptr = NULL;
		
	switch(type)
	{
		case MSG_GETCATALOG:
			{
				try
				{
					//��ȡ��ǰĿ¼���ļ����ļ�����Ϣ
					DebugPrintf("OnReceived.MSG_GETCATALOG: in MSG_GETCATALOG,file = [%s].\n",data);

					if(0 != strlen((char*)data))
					{
						dir = opendir((char*)data);
					}

					strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            		memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);

					if(dir == NULL)
					{
						printf( "OnReceived.MSG_GETCATALOG: file [%s] open error.(dir == NULL)\n", data);
            					memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
            					memcpy(strTemp, "?", 1 );
					
						if(!SendData((unsigned int)MSG_GETCATALOG,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
						{
							printf( "OnReceived.MSG_GETCATALOG: file [%s] error failed.(MSG_GETCATALOG_Exit)\n", data);	
						}

						printf( "OnReceived.MSG_GETCATALOG: file [%s] error.(MSG_GETCATALOG_Exit)\n", data);
            					free(strTemp);
						strTemp = NULL;
						
						if(dir == NULL)
						{
							closedir(dir);
						}
						return;
					}
					else
					{
						if((ptr = readdir(dir)) != NULL)
						{
							if((strcmp(ptr->d_name, ".") != 0) && (strcmp(ptr->d_name, "..") != 0))
							{
								if(IsFolder( (char*)data,ptr->d_name))
								{
									strcat( (char*)strTemp, "1" );
								}
								else
								{
									strcat( (char*)strTemp,"0" );
								}

								strcat( (char*)strTemp, ptr->d_name);
							}
						}
						else
						{
							printf( "OnReceived.MSG_GETCATALOG: file [%s] error.((ptr = readdir(dir)) == NULL)\n", data);
							
        					memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
        					memcpy(strTemp, "?", 1 );
						
							if(!SendData((unsigned int)MSG_GETCATALOG,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
							{
								printf( "OnReceived.MSG_GETCATALOG: file [%s] error failed.(MSG_GETCATALOG_Exit)\n", data);	
							}

							printf( "OnReceived.MSG_GETCATALOG: file [%s] error.(MSG_GETCATALOG_Exit)\n", data);
	            					free(strTemp);
							strTemp = NULL;
							
							if(dir == NULL)
							{
								closedir(dir);
							}
							return;
						}                

						while((ptr = readdir( dir )) != NULL)
						{
							if((strcmp(ptr->d_name, ".") == 0) || (strcmp(ptr->d_name, "..") == 0))
							{
								continue;
	            			}

							//ʣ��ռ��Ѿ��޷�д�뱾������,���ȷ���
							if(strlen((char*)strTemp )+1+1+strlen(ptr->d_name) >= NR_MAXPACKLEN )
							{
								if(!SendData((unsigned int)MSG_GETCATALOG,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
								{
									printf( "OnReceived.MSG_GETCATALOG: file [%s] send data failed.\n", data);
									
			            					memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
			            					memcpy(strTemp, "?", 1 );
								
									if(!SendData((unsigned int)MSG_GETCATALOG,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
									{
										printf( "OnReceived.MSG_GETCATALOG: file [%s] error failed.(MSG_GETCATALOG_Exit)\n", data);	
									}

									printf( "OnReceived.MSG_GETCATALOG: file [%s] error.(MSG_GETCATALOG_Exit)\n", data);
			            					free(strTemp);
									strTemp = NULL;
									
									if(dir == NULL)
									{
										closedir(dir);
									}
									return;
								}
	            						memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
							}

							if(strlen((char*)strTemp ) > 0)
							{
	                					strcat( (char*)strTemp, ":");
							}
	                
							if(IsFolder((char*)data,ptr->d_name))
							{
								strcat( (char*)strTemp, "1" );
							}
							else
							{
								strcat( (char*)strTemp,"0" );
							}

							strcat((char*)strTemp, ptr->d_name);
						}
					
						if(strlen((char*)strTemp ) > 0)
						{
	                				strcat( (char*)strTemp, "?");
						}
						else
						{
	            					memcpy(strTemp, "?", 1);
						}
					
						if(!SendData((unsigned int)MSG_GETCATALOG,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
						{
							printf( "OnReceived.MSG_GETCATALOG: file [%s] send data failed.\n", data);
							
	            					memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
	            					memcpy(strTemp, "?", 1 );
						
							if(!SendData((unsigned int)MSG_GETCATALOG,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
							{
								printf( "OnReceived.MSG_GETCATALOG: file [%s] error failed.(MSG_GETCATALOG_Exit)\n", data);	
							}

							printf( "OnReceived.MSG_GETCATALOG: file [%s] error.(MSG_GETCATALOG_Exit)\n", data);
	            					free(strTemp);
							strTemp = NULL;
							
							if(dir == NULL)
							{
								closedir(dir);
							}
							return;
						}

						free( strTemp );
						strTemp = NULL;
						if(dir == NULL)
						{
							closedir(dir);
						}
					}
				}
				catch(...)
				{
					printf("MSG_GETCATALOG:expection.\n"); 
				}
			}
			break;
		case MSG_CHECKFILEEXISTS:
			{
				//����ļ��Ƿ����
				try
				{
					DebugPrintf("in OnReceived.MSG_CHECKFILEEXISTS,file = [%s].\n",data);

					strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            				memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);

					if(access((char *)data,F_OK) == 0)
					{
            					memcpy(strTemp, "1", 1);
						DebugPrintf("OnReceived.MSG_CHECKFILEEXISTS:file = [%s] exists.\n",data);
					}
					else
					{
            					memcpy(strTemp, "0", 1);
						printf("OnReceived.MSG_CHECKFILEEXISTS:file = [%s] is not exists.\n",data);
					}

					if(!SendData((unsigned int)MSG_CHECKFILEEXISTS,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
					{
						printf("OnReceived.MSG_CHECKFILEEXISTS:send result failed.\n");
					}

					free( strTemp );
					strTemp = NULL;
				}
				catch(...)
				{
					printf("OnReceived.MSG_CHECKFILEEXISTS:expection.\n"); 
				}
			}
			break;
		case MSG_GETFILE:
			{
				//�����ļ�
				try
				{
					DebugPrintf("in OnReceived.MSG_GETFILE.\n");
				
					while(0 != m_threadDownFile)
					{
						sleep(5);
						DebugPrintf("OnReceived.MSG_GETFILE: 0 != m_threadDownFile .\n");
					}
				
					if(NULL != m_pGetFilePath)
					{
						free(m_pGetFilePath);
						m_pGetFilePath = NULL;
					}

					m_pGetFilePath = (char *)malloc(sizeof(char) * length);
            				memset(m_pGetFilePath, 0, sizeof(char) * length);
					memcpy(m_pGetFilePath,data,length);
				
					if(!pthread_create(&m_threadDownFile, NULL, DownFile_DoWork, this))
					{
						DebugPrintf("OnReceived.MSG_GETFILE:pthread_create create DownFile_DoWork Success!\n");
					}
					else
					{			
						printf("OnReceived.MSG_GETFILE:pthread_create create DownFile_DoWork failed!\n");
					}
				}
				catch(...)
				{
					printf("OnReceived.MSG_GETFILE:expection.\n"); 
				}
			}
			break;
		case MSG_GETFILE_CANCEL:
			{
				//�ļ�����ȡ��
				printf("in OnReceived.MSG_GETFILE_CANCEL: Get file cancel by user.\n");
				m_bIsGetFileCancel = true;
			}
			break;
		case MSG_SENDFILE:
			{
				try
				{
					//�ϴ��ļ�				
					if(NULL != m_fpSendFile)
					{
						//length == 0��ʱ��,��ʾ�����ϴ�����
						if((0 != length) && !m_bIsSendFileCancel)
						{
							int write_length = fwrite(data,sizeof(char),length,m_fpSendFile);
							if (write_length < length) 
							{
								//�������˳�����,���߿ͻ���,ȡ�������ϴ�
								if(!SendData((unsigned int)MSG_SENDFILE_CANCEL,MILLION))
								{
									printf("OnReceived.MSG_SENDFILE: Send MSG_SENDFILE_CANCEL failed.\n");								
								}
								
								printf("OnReceived.MSG_SENDFILE: error, write_length(%d) < length(%d).\n",write_length,length);

								if(NULL != m_fpSendFile)
								{
									fclose(m_fpSendFile);
									m_fpSendFile = NULL;
								}
								if(NULL != m_pSendFilePath)
								{
									remove(m_pSendFilePath);
									free(m_pSendFilePath);
									m_pSendFilePath = NULL;
								}
							}
						}
						else
						{
							if(NULL != m_fpSendFile)
							{
								fclose(m_fpSendFile);
								m_fpSendFile = NULL;
							}
							//������û�ȡ����,��ɾ���ļ�
							if(m_bIsSendFileCancel)
							{
								if(NULL != m_pSendFilePath)
								{
									remove(m_pSendFilePath);
								}
								DebugPrintf("OnReceived.MSG_SENDFILE: Send file cancle by user.\n");
							}
							else
							{
								struct stat info;
								int size = 0;
								if(stat(m_pSendFilePath, &info) >= 0)
								{
									size = info.st_size;
								}
				
								DebugPrintf("OnReceived.MSG_SENDFILE:file = [%s].length = %d \n",m_pSendFilePath,size);
								DebugPrintf("OnReceived.MSG_SENDFILE: Received[%s] finish.\n",m_pSendFilePath);
							}

							if(NULL != m_pSendFilePath)
							{
								free(m_pSendFilePath);
								m_pSendFilePath = NULL;
							}
						}
					}
					else
					{
						printf("OnReceived.MSG_SENDFILE: error,m_fpSendFile = NULL.\n"); 
					}
				}
				catch(...)
				{
					printf("OnReceived.MSG_SENDFILE:expection.\n"); 
				}
			}
			break;
		case MSG_SENDFILE_CANCEL:
			{
				//�ϴ��ļ�ȡ��
				DebugPrintf("OnReceived.MSG_SENDFILE_CANCEL: send file by user cancle.\n"); 
				m_bIsSendFileCancel = true;
			}
			break;
		case MSG_SENDFILENAME:
			{
				try
				{
					//�ϴ��ļ�������
					//����һ���µ��ļ�
					DebugPrintf("in OnReceived.MSG_SENDFILENAME,file = [%s].\n",data);
				
					if(NULL != m_fpSendFile)
					{
						fclose(m_fpSendFile);
						m_fpSendFile = NULL;
					}
					m_bIsSendFileCancel = false;
				
					if(NULL != m_pSendFilePath)
					{
						free(m_pSendFilePath);
						m_pSendFilePath = NULL;
					}

					m_pSendFilePath = (char *)malloc(sizeof(char) * length);
            				memset(m_pSendFilePath, 0, sizeof(char) * length);
					memcpy(m_pSendFilePath,data,length);

					char *pDirTemp = (char *)malloc(sizeof(char) * length);
            				memset(pDirTemp, 0, sizeof(char) * length);
					memcpy(pDirTemp,data,length);
					for(int i = length - 1;i > 0; i--)
					{
						if(pDirTemp[i] == '/')
						{
							pDirTemp[i] = '\0';
							break;
						}
					}

					int error = CreateDir(pDirTemp);
				
					strTemp = (char *)malloc(sizeof(char) * 10);
            				memset(strTemp, 0, sizeof(char) * 10);
				
					if(0 == error)
					{
						remove(m_pSendFilePath);					//��ɾ���ļ�,Ȼ���ٴ�����һ���µ�.

						m_fpSendFile = fopen(m_pSendFilePath,"a");		//�Ը��ӵķ�ʽ��ֻд�ļ������ļ������ڣ���Ὠ�����ļ�������ļ����ڣ�д������ݻᱻ�ӵ��ļ�β�����ļ�ԭ�ȵ����ݻᱻ��������EOF��������  

						if(NULL != m_fpSendFile)
						{
            						memcpy(strTemp, "1", 1);				//�ļ��򿪳ɹ�,���Է�������
							DebugPrintf("OnReceived.MSG_SENDFILENAME: m_fpSendFile open success.\n");
						}
						else
						{
            						memcpy(strTemp, "0", 1);				//�ļ���ʧ��,����������
							printf("OnReceived.MSG_SENDFILENAME: m_fpSendFile open filed.Error = %d .\n",errno);
						}
					}
					else
					{
            					memcpy(strTemp, "0", 1);				//�ļ���ʧ��,����������
						printf("OnReceived.MSG_SENDFILENAME: Create directory failed.Errno = %d .\n",errno);
					}

					if(!SendData((unsigned int)MSG_SENDFILE,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
					{
						printf("OnReceived.MSG_SENDFILENAME: Send MSG_SENDFILE failed.\n");
					}
				
					free( strTemp );
					strTemp = NULL;
				}
				catch(...)
				{
					printf("OnReceived.MSG_SENDFILENAME:expection.\n"); 
				}
			}
			break;
		case MSG_GETFILELENGTH:
			{
				try
				{
					//��ȡ�ļ�����
					DebugPrintf("in OnReceived.MSG_GETFILELENGTH,file = [%s].\n",data);

					strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            				memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);

					struct stat info;
					int size = 0;
					if(stat((char *)data, &info) >= 0)
					{
						size = info.st_size;
					}

					DebugPrintf("OnReceived.MSG_GETFILELENGTH: file = [%s].length = %d \n",data,size);
					sprintf(strTemp, "%d", size);
					if(!SendData((unsigned int)MSG_GETFILELENGTH,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
					{
						printf("OnReceived.MSG_GETFILELENGTH: Send MSG_GETFILELENGTH failed.\n");
					}

					free( strTemp );
					strTemp = NULL;
				}
				catch(...)
				{
					printf("OnReceived.MSG_GETFILELENGTH: expection.\n"); 
				}
			}
			break;
		case MSG_GETFILECREATEDATE:
			{
				try
				{
					//��ȡ�ļ�����ʱ��
					DebugPrintf("in OnReceived.MSG_GETFILECREATEDATE,file = [%s].\n",data);

					strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            		memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
					PTIME pTime = (PTIME)strTemp;
					
					struct stat info;
					if(stat((char *)data, &info) >= 0)
					{
						struct tm *newtime = localtime(&(info.st_atime));
						pTime->year = newtime->tm_year;
						pTime->month = newtime->tm_mon;
						pTime->day = newtime->tm_mday;
						pTime->hour = newtime->tm_hour;
						pTime->minute= newtime->tm_min;
						pTime->second= newtime->tm_sec;

						DebugPrintf("OnReceived.MSG_GETFILECREATEDATE: TIME,%04d-%02d-%02d %02d:%02d:%02d .\n", newtime->tm_year + 1900,newtime->tm_mon + 1,newtime->tm_mday,newtime->tm_hour,newtime->tm_min,newtime->tm_sec);
						//sprintf(strTemp, "%04d-%02d-%02d %02d:%02d:%02d", newtime->tm_year + 1900,newtime->tm_mon + 1,newtime->tm_mday,newtime->tm_hour,newtime->tm_min,newtime->tm_sec);
					}
					
					if(!SendData((unsigned int)MSG_GETFILECREATEDATE,(unsigned char *)strTemp,(unsigned int)(sizeof(*pTime)),MILLION))
					{
						printf("OnReceived.MSG_GETFILECREATEDATE: Send MSG_GETFILECREATEDATE failed.\n"); 
					}

					free( strTemp );
					strTemp = NULL;
				}
				catch(...)
				{
					printf("OnReceived.MSG_GETFILECREATEDATE: expection.\n"); 
				}
			}
			break;
			
		case MSG_GETFILEINFO:
			{
				try
				{
					//��ȡ�ļ�����ʱ����ļ�����
					DebugPrintf("in OnReceived.MSG_GETFILEINFO,file = [%s].\n",data);

					strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            		memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
					PFILEINFO pFileInfo = (PFILEINFO)strTemp;
					
					struct stat info;
					FILEINFO fInfo;
					if(stat((char *)data, &info) >= 0)
					{
						struct tm *newtime = localtime(&(info.st_atime));  
						pFileInfo->time.year = newtime->tm_year;
						pFileInfo->time.month = newtime->tm_mon;
						pFileInfo->time.day = newtime->tm_mday;
						pFileInfo->time.hour = newtime->tm_hour;
						pFileInfo->time.minute= newtime->tm_min;
						pFileInfo->time.second= newtime->tm_sec;
						pFileInfo->length = info.st_size;
						
						DebugPrintf("OnReceived.MSG_GETFILEINFO: TIME, %04d-%02d-%02d %02d:%02d:%02d,Length = %d. \n", newtime->tm_year + 1900,newtime->tm_mon + 1,newtime->tm_mday,newtime->tm_hour,newtime->tm_min,newtime->tm_sec,info.st_size);

						//sprintf(strTemp, "%04d-%02d-%02d %02d:%02d:%02d", newtime->tm_year + 1900,newtime->tm_mon + 1,newtime->tm_mday,newtime->tm_hour,newtime->tm_min,newtime->tm_sec);
						//size = info.st_size;
					}
				
					if(!SendData((unsigned int)MSG_GETFILEINFO,(unsigned char *)strTemp,(unsigned int)(sizeof(*pFileInfo)),MILLION))
					{
						printf("OnReceived.MSG_GETFILEINFO: Send MSG_GETFILEINFO failed.\n"); 
					}

					free( strTemp );
					strTemp = NULL;
				}
				catch(...)
				{
					printf("OnReceived.MSG_GETFILEINFO: expection.\n"); 
				}
			}
			break;
		case MSG_GETSERIALNUMBER:
			{
				try
				{
					//2013-05-08,jiang,������������Ժ�ʹ�������MSG_GETSERIALNUMBERALL ��
					//������ص����к�ȫ��Ϊ14λ��������ģ������кŲ��ȳ������в����ϣ��޷�ʹ��
									
					//2013-02-20,jiang,���ӷ��ظ���ģ�����кŵĹ���
					//�ɰ汾ֻ��������������к�,����Ϊ14,
					//�°汾���������кŰ�ÿ��14λֱ�������һ�𷵻�,�м�û�м����

					//��ȡ���������к�
					DebugPrintf("in OnReceived.MSG_GETSERIALNUMBER\n");
					int iRet = 0;
					strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            				memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);	
					char sn[256] = {'\0'};
					int iStrTempIndex = 0;

					//��ȡ��������к�
					unsigned char pSerNumber[14] = {'\0'}; 
					if(iRet = ReadSerialNumber(pSerNumber))
					{
						printf("OnReceived.MSG_GETSERIALNUMBER: Read Serial No from flash fail, errno:%d!\n", iRet);
					}
					else
					{
						DebugPrintf("OnReceived.MSG_GETSERIALNUMBER: Serial Number: %s \n",pSerNumber);
					}
					memcpy(strTemp + iStrTempIndex,pSerNumber,14);
					iStrTempIndex += 14;

					//��ȡ����ģ������к�
					for(int board_index = 0; board_index < V400PANEL_BOARDNUM; board_index++)
					{
						try
						{
							memset(sn,0,256);							
							if(0 == v400_boardres_ind2sn(board_index,sn))
							{
								DebugPrintf("OnReceived.MSG_GETSERIALNUMBER: sn = %s,index = %d\n",sn,board_index);
							}
							else
							{
								printf("OnReceived.MSG_GETSERIALNUMBER:v400_boardres_ind2sn failed.");
							}
									
							memcpy(strTemp + iStrTempIndex,sn,14);
							iStrTempIndex += 14;
						}
						catch(...)
						{
							printf("OnReceived.MSG_GETMAC: v400_boardres_ind2sn expection.\n"); 
						}
					}
				
					if(!SendData((unsigned int)MSG_GETSERIALNUMBER,(unsigned char *)strTemp,iStrTempIndex,MILLION))
					{
						printf("OnReceived.MSG_GETSERIALNUMBER: send MSG_GETSERIALNUMBER failed.\n"); 
					}

					free( strTemp );
					strTemp = NULL;
				}
				catch(...)
				{
					printf("OnReceived.MSG_GETSERIALNUMBER: expection.\n"); 
				}
			}
			break;			
		case MSG_GETSERIALNUMBERALL:
			{
				try
				{
					//2013-05-08,jiang,�µĻ�ȡ�������кŷ�����ÿ�����к�֮����"\r"���
					//��ʽ:�������к�+ "\r" + ģ�����к�+ "\r" +...... 
					
					//��ȡ���������к�
					DebugPrintf("in OnReceived.MSG_GETSERIALNUMBERALL\n");
					int iRet = 0;
					strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            				memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);	
					char sn[256] = {'\0'};
					int iStrTempIndex = 0;

					//��ȡ��������к�
					unsigned char pSerNumber[14] = {'\0'}; 
					if(iRet = ReadSerialNumber(pSerNumber))
					{
						printf("OnReceived.MSG_GETSERIALNUMBERALL: Read Serial No from flash fail, errno:%d!\n", iRet);
					}
					else
					{
						DebugPrintf("OnReceived.MSG_GETSERIALNUMBERALL: Serial Number: %s \n",pSerNumber);
					}
					memcpy(strTemp + iStrTempIndex,pSerNumber,14);
					iStrTempIndex += 14;
					strTemp[iStrTempIndex++] = '\r'; 

					//��ȡ����ģ������к�
					for(int board_index = 0; board_index < V400PANEL_BOARDNUM; board_index++)
					{
						try
						{
							memset(sn,0,256);							
							if(0 == v400_boardres_ind2sn(board_index,sn))
							{
								DebugPrintf("OnReceived.MSG_GETSERIALNUMBERALL: sn = %s,index = %d\n",sn,board_index);
							}
							else
							{
								printf("OnReceived.MSG_GETSERIALNUMBERALL:v400_boardres_ind2sn failed.");
							}
							
							memcpy(strTemp + iStrTempIndex,sn,strlen((char *)sn));
							iStrTempIndex += strlen((char *)sn);
							strTemp[iStrTempIndex++] = '\r'; 
						}
						catch(...)
						{
							printf("OnReceived.MSG_GETSERIALNUMBERALL: v400_boardres_ind2sn expection.\n"); 
						}
					}
				
					if(!SendData((unsigned int)MSG_GETSERIALNUMBERALL,(unsigned char *)strTemp,iStrTempIndex,MILLION))
					{
						printf("OnReceived.MSG_GETSERIALNUMBERALL: send MSG_GETSERIALNUMBERALL failed.\n"); 
					}

					free( strTemp );
					strTemp = NULL;
				}
				catch(...)
				{
					printf("OnReceived.MSG_GETSERIALNUMBERALL: expection.\n"); 
				}
			}
			break;
		case MSG_SETSERIALNUMBER:
			{
				try
				{
					//���ñ��������к�
					DebugPrintf("in OnReceived.MSG_SETSERIALNUMBER: %s\n",data);
				
					int iRet = WriteSerialNumber((unsigned char *)data);
				
					DebugPrintf("OnReceived.MSG_SETSERIALNUMBER: error = %d\n",iRet);
				
					strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            				memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
					sprintf(strTemp,"%d",iRet);
				
					if(!SendData((unsigned int)MSG_SETSERIALNUMBER,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
					{
						printf("OnReceived.MSG_SETSERIALNUMBER: send MSG_SETSERIALNUMBER failed.\n"); 
					}

					free( strTemp );
					strTemp = NULL;
				}
				catch(...)
				{
					printf("OnReceived.MSG_SETSERIALNUMBER: expection.\n"); 
				}
			}
			break;
		case MSG_GETMAC:
			{
				try
				{
					//��ȡ������MAC
					//2013-02-20,jiang,��ԭ��ֻ����һ��MAC��ַ��Ϊ��������ģ���MAC��ַ,40G,2.5Gû��MAC��ַ
					//�ɰ汾ֻ����һ��MAC��ַʱ,�������ݳ���Ϊ6
					//�°汾ֱ�ӷ���ȫ����MAC��ַ,ÿ��MAC��ַ����Ϊ6λ,�м�û�м����
					DebugPrintf("in OnReceived.MSG_GETMAC\n");
					
					int iRet = 0;		
					int iType = 0;
					strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            				memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
					unsigned char pMAC[6] = {'\0'};
					int iStrTempIndex = 0;
					//��ȡ�����MAC��ַ
					if(iRet = ReadMAC(pMAC))
					{
						printf("OnReceived.MSG_GETMAC: Read Serial No from flash fail, errno:%d!\n", iRet);
					}
					else
					{
						DebugPrintf("OnReceived.MSG_GETMAC: Mac: %s \n",pMAC);
					}
					memcpy(strTemp + iStrTempIndex,pMAC,6);
					iStrTempIndex += 6;
					
					//��ȡ����ģ���MAC��ַ
					for(int board_index = 0; board_index < V400PANEL_BOARDNUM; board_index++)
					{
						try
						{
							iType = GetBoardresType(board_index);
						}
						catch(...)
						{
							printf("OnReceived.MSG_GETMAC: GetBoardresType expection.\n"); 
						}

						memset(pMAC,0,6);

						if((iType != eV400_board_40g) &&  (iType != eV400_board_2g5))
						{
							try
							{
								unsigned char tempBuffer[255] = {'\0'}; 
								iRet = ReadOptionBufferIndex(tempBuffer,0,6,board_index);
								printf("OnReceived.MSG_GETMAC: ReadOptionBufferIndex finish.\n"); 
								memcpy(pMAC,tempBuffer,6);

								if(0 != iRet)
								{
									printf("OnReceived.MSG_GETMAC:ReadOptionBufferIndex failed.ErrorCode = %d,index = %d.\n",iRet,board_index);
								}
							}
							catch(...)
							{
								printf("OnReceived.MSG_GETMAC: ReadOptionBufferIndex expection.\n"); 
							}
						}

						memcpy(strTemp + iStrTempIndex,pMAC,6);
						iStrTempIndex += 6;
					}
					
					if(!SendData((unsigned int)MSG_GETMAC,(unsigned char *)strTemp,iStrTempIndex,MILLION))
					{
						printf("OnReceived.MSG_GETMAC: Send MSG_GETMAC failed.\n"); 					
					}

					free( strTemp );
					strTemp = NULL;
				}
				catch(...)
				{
					printf("OnReceived.MSG_GETMAC: expection.\n"); 
				}
			}
			break;
		case MSG_SETMAC:
			{
				try
				{
					//���ñ�����MAC
					DebugPrintf("in OnReceived.MSG_SETMAC\n");
				
					int iRet = WriteMAC((unsigned char *)data);
				
					DebugPrintf("OnReceived.MSG_SETMAC: error = %d\n",iRet);
				
					strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            				memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
					sprintf(strTemp,"%d",iRet);
				
					if(!SendData((unsigned int)MSG_SETMAC,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
					{
						printf("OnReceived.MSG_SETMAC: Send MSG_SETMAC failed.\n"); 
					}

					free( strTemp );
					strTemp = NULL;
				}
				catch(...)
				{
					printf("OnReceived.MSG_SETMAC: expection.\n"); 
				}
			}
			break;
		case MSG_READMEMORY_CANCEL:
			{
				printf("in OnReceived.MSG_READMEMORY_CANCEL\n");
				m_bIsReadMemoryCancel = true;
			}
			break;
		case MSG_READMEMORY:
			{
				try
				{
					//��ȡ�ڴ�
					DebugPrintf("in OnReceived.MSG_READMEMORY\n");

					while(0 != m_threadReadMemory)
					{
						sleep(5);
						DebugPrintf("OnReceived.MSG_READMEMORY: 0 != m_threadReadMemory .\n");
					}

					m_memInfoRead = (*(MEMINFO *)data);

					if(!pthread_create(&m_threadReadMemory, NULL, ReadMemory_DoWork, this))
					{
						DebugPrintf("OnReceived.MSG_READMEMORY: pthread_create create ReadMemory_DoWork Success!\n");
					}
					else
					{			
						printf("OnReceived.MSG_READMEMORY: pthread_create create ReadMemory_DoWork failed!\n");
					}
				}
				catch(...)
				{
					printf("OnReceived.MSG_READMEMORY: expection.\n"); 
				}
			}
			break;
		case MSG_WRITEMEMORY:
			{
				try
				{
					//д���ڴ�
					DebugPrintf("in OnReceived.MSG_WRITEMEMORY\n");
					MEMDATA *pMemData;
					pMemData->stMemInfo = (*(MEMINFO *)data);
					pMemData->pucBuf = (UCHAR *)(data + sizeof(MEMINFO));
				
					DebugPrintf("OnReceived.MSG_WRITEMEMORY: Write addr = 0x%x,len = %d,conent = %s.\n",pMemData->stMemInfo.ulStartAddr,pMemData->stMemInfo.ulLength,pMemData->pucBuf);
					int iRet = 0;
					//iRet = WriteMemory(pMemData);
				
					strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            				memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
					sprintf(strTemp,"%d",iRet);
				
					if(!SendData((unsigned int)MSG_WRITEMEMORY,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
					{
						printf("OnReceived.MSG_WRITEMEMORY: Send MSG_WRITEMEMORY failed.\n"); 
					}

					free( strTemp );
					strTemp = NULL;
				}
				catch(...)
				{
					printf("OnReceived.MSG_WRITEMEMORY: expection.\n"); 
				}
			}
			break;
		case MSG_CMD:
			{
				try
				{
					//ִ������
					DebugPrintf("in OnReceived.MSG_CMD\n");
				
					while(0 != m_threadCMD)
					{
						sleep(5);
						DebugPrintf("OnReceived.MSG_CMD: 0 != MSG_CMD .\n");
					}
					
					m_CMD = (char *)malloc(sizeof(char) * length);
            				memset(m_CMD, 0, sizeof(char) * length);
					memcpy(m_CMD,data,length);
					
					DebugPrintf("OnReceived.MSG_CMD: CMD,%s.\n",m_CMD);
					if(!pthread_create(&m_threadCMD, NULL, CMD_DoWork, this))
					{
						DebugPrintf("OnReceived.MSG_CMD: pthread_create create CMD_DoWork Success!\n");
					}
					else
					{			
						printf("OnReceived.MSG_CMD: pthread_create create CMD_DoWork failed!\n");
					}
				}
				catch(...)
				{
					printf("OnReceived.MSG_CMD: expection.\n"); 
				}
			}
			break;
		case MSG_CMD_CANCEL:
			{
				//ȡ������ִ�����������
				DebugPrintf("in OnReceived.MSG_CMD_CANCEL\n");
				m_bIsCMDCancel = true;
			}
			break;
		case MSG_MEMORYSCRIPT_START:
			{
				//׼�����͵��ڴ�ű��ĳ���
				DebugPrintf("in OnReceived.MSG_MEMORYSCRIPT_START,length = %d\n",length);
				
				bool success = false;
				try
				{
					if(m_bIsMemoryScriptFinish)
					{
						m_bIsMemoryScriptCancel = false;
						m_MemoryScriptLength = (ULONG)(atoi((char *)data));
						m_MemoryScript = (char *)malloc(sizeof(char) * m_MemoryScriptLength);
						m_MemoryScript[0] = '\0';
						success = true;
					}
				}
				catch(...)
				{
					printf("OnReceived.MSG_MEMORYSCRIPT_START: expection.\n"); 
				}

				strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            			memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
				sprintf(strTemp,"%d",(success ? 0 : 1));
				if(!SendData((unsigned int)MSG_MEMORYSCRIPT_START,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
				{
					printf("OnReceived.MSG_MEMORYSCRIPT_START: Send MSG_MEMORYSCRIPT_START failed.\n"); 
				}

				free(strTemp);
				strTemp = NULL;
			}
			break;
		case MSG_MEMORYSCRIPT:
			{
				//��ʼ�����ڴ�ű�,
				DebugPrintf("in OnReceived.MSG_MEMORYSCRIPT: length = %d,m_MemoryScriptLength = %d.\n",length,m_MemoryScriptLength);

				try
				{
					m_bIsMemoryScriptFinish = false;

					if(!m_bIsMemoryScriptCancel)
					{
						if(null != m_MemoryScript)
						{
							int iLen = strlen(m_MemoryScript);
							strcat(m_MemoryScript,(char *)data);
							iLen += length;
							m_MemoryScript[iLen] = '\0';
							
							if(iLen == m_MemoryScriptLength)
							{
								if(!pthread_create(&m_threadMemoryScript, NULL, MemoryScript_DoWork, this))
								{
									DebugPrintf("OnReceived.MSG_MEMORYSCRIPT: pthread_create create MemoryScript_DoWork Success!\n");
								}
								else
								{
									printf("OnReceived.MSG_MEMORYSCRIPT: pthread_create create MemoryScript_DoWork failed!\n");
								}
							}
						}
					}
					else
					{
						m_bIsMemoryScriptFinish = true;
					}
				}
				catch(...)
				{
					printf("OnReceived.MSG_MEMORYSCRIPT: expection.\n"); 
				}
			}
			break;
		case MSG_MEMORYSCRIPT_CANCEL:
			{
				//ȡ���ڴ�ű���ִ��
				DebugPrintf("in OnReceived.MSG_MEMORYSCRIPT_CANCEL\n");
				
				m_bIsMemoryScriptCancel = true;
				m_bIsMemoryScriptFinish = true;
			}
			break;
		case MSG_GETBOARDTYPE:
			{
				//��ȡģ������
				DebugPrintf("in OnReceived.MSG_GETBOARDTYPE,index = %s\n",data);
				
				int index = atoi((char *)data);
				int iType = 0;
				try
				{
					iType = GetBoardresType(index);
				}
				catch(...)
				{
					printf("OnReceived.MSG_GETBOARDTYPE: expection.\n"); 
				}

				DebugPrintf("OnReceived.MSG_GETBOARDTYPE:iType = %d.\n",iType);
				strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            			memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
				sprintf(strTemp,"%d",iType);
				if(!SendData((unsigned int)MSG_GETBOARDTYPE,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
				{
					printf("OnReceived.MSG_GETBOARDTYPE: expection.\n"); 
				}

				free(strTemp);
				strTemp = NULL;
			}
			break;
		case MSG_READMODULEBUF:
			{
				//��ģ��
				DebugPrintf("in OnReceived.MSG_READMODULEBUF\n");

				try
				{
					PMODULEINFO pModuleInfo = (PMODULEINFO)data;
					DebugPrintf("OnReceived.MSG_READMODULEBUF: iStartAddr = %d,iLength = %d,iIndex = %d \n",pModuleInfo->iStartAddr,pModuleInfo->iLength,pModuleInfo->iIndex);

					int iRet = 0;		//Success
					unsigned char pBuff[EEPROM_MAXSIZE] = {'\0'};
					memset(pBuff,0,EEPROM_MAXSIZE);
					if(pModuleInfo->iStartAddr + pModuleInfo->iLength <= 255)
					{
						iRet = ReadModuleBuf(pModuleInfo,pBuff);

						if(0 != iRet)
						{
							printf("OnReceived.MSG_READMODULEBUF:ReadModuleBuf failed.ErrorCode = %d.\n",iRet);
						}
					}
					else
					{
						iRet = 1;		//Parameter invalid
						printf("OnReceived.MSG_READMODULEBUF: Parameter is invalid.\n");
					}

					DebugPrintf("OnReceived.MSG_READMODULEBUF:  iRet = %d.\n",iRet);
					
					strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            				memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);							

					int iLen = 0;
					sprintf(strTemp,"%d,",iRet);
					iLen = strlen(strTemp);
					memcpy(strTemp + iLen,pBuff,pModuleInfo->iLength);
					iLen += pModuleInfo->iLength;

					DebugPrintf("OnReceived.MSG_READMODULEBUF: strTemp = %s,iLen = %d,strlen(strTemp) = %d.\n",strTemp,iLen,strlen(strTemp));
					
					if(!SendData((unsigned int)MSG_READMODULEBUF,(unsigned char *)strTemp,iLen,MILLION))
					{
						printf("OnReceived.MSG_READMODULEBUF: Send MSG_READMODULEBUF failed.\n"); 
					}

					free(strTemp);
					strTemp = NULL;		
				}
				catch(...)
				{
					printf("OnReceived.MSG_READMODULEBUF: expection.\n"); 
				}
			}
			break;
		case MSG_WRITEMODULEBUF:
			{
				//дģ��
				DebugPrintf("in OnReceived.MSG_WRITEMODULEBUF,data = %s.\n",data);
				
				try
				{
					PMODULEINFO pModuleInfo = (PMODULEINFO)data;
					DebugPrintf("OnReceived.MSG_WRITEMODULEBUF: iStartAddr = %d,iLength = %d,iIndex = %d ,length = %d,sizeof(MODULEINFO) = %d.\n",pModuleInfo->iStartAddr,pModuleInfo->iLength,pModuleInfo->iIndex,length,sizeof(MODULEINFO));
					
					int iRet = 0;
										
					if((pModuleInfo->iStartAddr + pModuleInfo->iLength <= 255) && (pModuleInfo->iLength + sizeof(MODULEINFO) <= length))
					{
						unsigned char *pBuff = (unsigned char *)malloc(sizeof(char) * (pModuleInfo->iLength));
						memset(pBuff,0,pModuleInfo->iLength);
						memcpy(pBuff,data + sizeof(MODULEINFO),pModuleInfo->iLength);
					
						iRet = WriteModuleBuf(pModuleInfo,pBuff);
						
						free(pBuff);
						pBuff = NULL;
					}
					else
					{
						iRet = 1;		//Parameter invalid
						printf("OnReceived.MSG_WRITEMODULEBUF: Parameter is invalid.\n");
					}

					strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            				memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
					sprintf(strTemp,"%d",iRet);
					if(!SendData((unsigned int)MSG_WRITEMODULEBUF,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
					{
						printf("OnReceived.MSG_WRITEMODULEBUF: Send MSG_WRITEMODULEBUF failed.\n"); 
					}

					free(strTemp);
					strTemp = NULL;
				}
				catch(...)
				{
					printf("OnReceived.MSG_WRITEMODULEBUF: expection.\n"); 
				}
			}
			break;
			
		case MSG_WRITEOPTION:
			{
				//ģ��λд��
				DebugPrintf("in OnReceived.MSG_WRITEOPTION\n");
				
				try
				{
					POPTIONINFO pOptionInfo = (POPTIONINFO)data;
					DebugPrintf("OnReceived.MSG_WRITEOPTION: size = %d,  offset = %d, pass = %s,data = %d,iIndex = %d \n",sizeof(pOptionInfo),pOptionInfo->optTableInfo.offset,pOptionInfo->optTableInfo.szPwd,pOptionInfo->data,pOptionInfo->index);
					
					int iRet = 0;
					
					iRet = WriteOptionBit(pOptionInfo);					
					
					strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            				memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
					sprintf(strTemp,"%d",iRet);
					if(!SendData((unsigned int)MSG_WRITEOPTION,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
					{
						printf("OnReceived.MSG_WRITEOPTION: Send MSG_WRITEOPTION failed.\n"); 
					}

					free(strTemp);
					strTemp = NULL;
				}
				catch(...)
				{
					printf("OnReceived.MSG_WRITEOPTION: expection.\n"); 
				}
			}
			break;
		case MSG_GETVERSION:
			{
				//��ȡ����汾��
				DebugPrintf("in OnReceived.MSG_GETVERSION\n");
				
				strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            	memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);
				sprintf(strTemp,"%d",VERSION);
				
				DebugPrintf("Current Version = [%s]\n",strTemp);
				if(!SendData((unsigned int)MSG_GETVERSION,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
				{
					printf("OnReceived.MSG_GETVERSION: expection.\n"); 
				}
				
				free(strTemp);
				strTemp = NULL;
			}
			break;
		case MSG_GETALLFILEINFO:
			{
				//��ȡ��·�����ļ���ϸ��Ϣ
				//������ʽ˵��:����·��+":"+����+"?"+����·��+":"+����+"?"......,(�����ù̶�����,�������Ը��õ����ô���)
				//����ֵ��ʽ˵��:   ���ļ���+":"+����+":"+ʱ��+":"+����+"\r"+���ļ���+":"+����+":"+ʱ��+":"+����+"\r"........+���ļ���+":"+����+":"+ʱ��+":"+����+"\r"+"?"
        		//             ����ֵ��"?"����.
        		//				����ʱ���ʽ:[YYYY-MM-DD-HH-MM-SS],��1900��1��Ϊ���
				DebugPrintf("in OnReceived.MSG_GETALLFILEINFO,path = %s.\n",data);

				strTemp = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
            	memset(strTemp, 0, sizeof(char) * NR_MAXPACKLEN);

				char *path = (char *)malloc(sizeof(char) * MAX_FILENAME_LEN);
				char *fileType = (char *)malloc(sizeof(char) * MAX_FILENAME_LEN);
				
				int iDataLen = strlen((char*)data);
				int j = 0;
				
			    DIR *dir = NULL;
				struct dirent *ptr = NULL;
				struct stat info;
				//������ʱ��ȡ���ļ���Ϣ
				char *fileInfo = (char *)malloc(sizeof(char) * NR_MAXPACKLEN);
				char *fileName = (char *)malloc(MAX_FILENAME_LEN);
				PTIME pTime = NULL;
				char *tempTime = (char *)malloc(sizeof(char) * 40);
				
				for(int i = 0; i < iDataLen; i++)
				{
					//path
					memset(path,0,MAX_FILENAME_LEN);
					j = 0;
					for( ; i < iDataLen && j < MAX_FILENAME_LEN; i++)
					{
						if(':' == data[i])
						{
							i++;
							break;
						}
						else
						{
							path[j++] = data[i];
						}
					}
					DebugPrintf("\nPath = [%s].\n",path);

					//type
					memset(fileType,0,MAX_FILENAME_LEN);
					j = 0;
					for( ; i < iDataLen && j < MAX_FILENAME_LEN; i++)
					{
						if('?' == data[i])
						{
							break;
						}
						else
						{
							fileType[j++] = data[i];
						}
					}
					DebugPrintf("Type = [%s].\n",fileType);

					if((0 < strlen(path)) && (0 < strlen(fileType)))
					{
					    dir = NULL;
						ptr = NULL;

						try
						{
							if(0 != strlen((char*)path))
							{
								dir = opendir((char*)path);
							}

							if(dir != NULL)
							{
								int iState = -1;
							
								while((ptr = readdir( dir )) != NULL)
								{
									if((strcmp(ptr->d_name, ".") == 0) || (strcmp(ptr->d_name, "..") == 0))
									{
										continue;
					    			}

									//FileName
									memset(fileName,0,MAX_FILENAME_LEN);
									strcpy(fileName,path);
									strcat(fileName,"/");
									strcat(fileName,ptr->d_name);
									DebugPrintf("Name = [%s].\n",fileName);
									
									//��ȡ�ļ�����ϸ��Ϣ
									iState = -1;
									iState = stat(fileName,&info);
									//������ļ���������
									if(((iState == 0) && (info.st_mode & S_IFDIR)))
									{
										DebugPrintf("Name = [%s] is folder.\n",fileName);
										continue;				
									}

									memset(fileInfo,0,NR_MAXPACKLEN);
									//name
									strcpy(fileInfo,ptr->d_name);
									strcat(fileInfo,":");
									
									//type
									strcat(fileInfo,fileType);
									strcat(fileInfo,":");
									//time
									struct tm *newtime = localtime(&(info.st_atime));
									memset(tempTime,0,40);
									#if 0
									pTime = (PTIME)(tempTime);								
									pTime->year = newtime->tm_year;
									pTime->month = newtime->tm_mon;
									pTime->day = newtime->tm_mday;
									pTime->hour = newtime->tm_hour;
									pTime->minute= newtime->tm_min;
									pTime->second= newtime->tm_sec;
									DebugPrintf("Date = [%04d-%02d-%02d %02d:%02d:%02d]\n", newtime->tm_year + 1900,newtime->tm_mon + 1,newtime->tm_mday,newtime->tm_hour,newtime->tm_min,newtime->tm_sec);
									
									strcat(fileInfo,tempTime);
									strcat(fileInfo,":");
									//length
									memset(tempTime,0,40);
									sprintf(tempTime,"%d",info.st_size);
									strcat(fileInfo,tempTime);
									strcat(fileInfo,"\r");
									#endif

									sprintf(tempTime,"%04d-%02d-%02d-%02d-%02d-%02d:%d\r",newtime->tm_year,newtime->tm_mon,newtime->tm_mday,newtime->tm_hour,newtime->tm_min,newtime->tm_sec,info.st_size);
									
									strcat(fileInfo,tempTime);
									
									DebugPrintf("Info = [%s]\n",fileInfo);
										
									//����������,��������
									if(strlen(strTemp) + strlen(fileInfo) > NR_MAXPACKLEN )
									{
										if(!SendData((unsigned int)MSG_GETALLFILEINFO,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
										{
											printf("OnReceived.MSG_GETALLFILEINFO: Send MSG_GETALLFILEINFO failed.\n"); 

											memset(strTemp,0,NR_MAXPACKLEN);
											memcpy(strTemp,"?",1);

											SendData((unsigned int)MSG_GETALLFILEINFO,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION);

											return;
										}

										memset(strTemp,0,NR_MAXPACKLEN);
									}

									if(0 == strlen(strTemp))
									{
										strcpy(strTemp,fileInfo);
									}
									else
									{
										strcat(strTemp,fileInfo);
									}
								}//while((ptr = readdir( dir )) != NULL)								

								if(dir != NULL)
								{
									closedir(dir);
								}
							}
							else
							{
								DebugPrintf("Path = [%s] is open failed.\n",path);
							}
						}
						catch(...)
						{
							if(dir != NULL)
							{
								closedir(dir);
							}
							dir = NULL;
							printf("MSG_GETCATALOG:expection.\n"); 
						}
					}
					
					DebugPrintf("Path = [%s] is finish.\n",path);
				}//for(int i = 0; i < iDataLen; i++)
				
				//����������,��������
				if(strlen(strTemp) + 1 > NR_MAXPACKLEN )
				{
					if(!SendData((unsigned int)MSG_GETALLFILEINFO,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
					{
						printf("OnReceived.MSG_GETALLFILEINFO: Send MSG_GETALLFILEINFO failed.\n"); 

						memset(strTemp,0,NR_MAXPACKLEN);
						memcpy(strTemp,"?",1);

						SendData((unsigned int)MSG_GETALLFILEINFO,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION);

						return;
					}

					memset(strTemp,0,NR_MAXPACKLEN);
				}

				//���ͽ������
				if(0 == strlen(strTemp))
				{
					strcpy(strTemp,"?");
				}
				else
				{
					strcat(strTemp,"?");
				}

				if(!SendData((unsigned int)MSG_GETALLFILEINFO,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION))
				{
					printf("OnReceived.MSG_GETALLFILEINFO: Send MSG_GETALLFILEINFO failed.\n"); 

					memset(strTemp,0,NR_MAXPACKLEN);
					memcpy(strTemp,"?",1);

					SendData((unsigned int)MSG_GETALLFILEINFO,(unsigned char *)strTemp,(unsigned int)(strlen((char *)strTemp)),MILLION);

					return;
				}
				
				DebugPrintf("MSG_GETALLFILEINFO is finish.\n",path);
			}
			break;
		default:
			{
				//�����յ�δ��������ʱ,��������ֱ�ӷ���,����Ϊ��,�������Ͷ˲��õȴ���ʱ
				DebugPrintf("in OnReceived.UNKOWNCMD.\n");
				
				if(!SendData(type,null,0,MILLION))
				{
					printf("OnReceived.UNKOWNCMD: expection.\n"); 
				}
			}
			break;
	}
}

 void CCommonServer::OnServerClose()
{
	DebugPrintf("Into CCommonServer::OnServerClose!\n");

	
	DebugPrintf("Leaving CCommonServer::OnServerClose!\n");
}

void CCommonServer::OnSocketError()
{
	DebugPrintf("Into CCommonServer::OnSocketError!\n");

	
	DebugPrintf("Leaving CCommonServer::OnSocketError!\n");
}

bool CCommonServer::GetIsConnection()
{
	return ClientBase::KeepReceiving();
}
