
/*****************************************************************************************
** CommonServer.h: 用来与远程建立连接的客户端
** 负责与远程连接进行交互
** Author: cjiang
** Copyright (C) 2012 VeTronics(BeiJing) Ltd.
**
** Create date: 05-03-2012
*****************************************************************************************/

#pragma once
#include "../clientbase/clientbase.h"
#include <pthread.h>					//pthread_t,pthread_exit,pthread_create

class CCommonServer : public ClientBase
{
public:
	CCommonServer(void);
	~CCommonServer(void);

	bool m_bIsGetFileCancel;					//文件下载取消的标记
	bool m_bIsSendFileCancel;					//文件上传取消的标记
	FILE *m_fpSendFile;							//上传文件时本地文件句柄
	char *m_pSendFilePath;						//上传文件时本地路径
	char *m_pGetFilePath;						//下载文件时本地路径
	pthread_t m_threadDownFile;					//下载文件的线程
	pthread_t m_threadReadMemory;				//读取内存的线程
	MEMINFO m_memInfoRead;						//读取内存时的参数
	bool m_bIsReadMemoryCancel;					//读取内存取消的标记
	bool m_bIsCMDCancel;						//执行命令后的返回值取消的标记
	pthread_t m_threadCMD;						//执行命令后的返回值
	char *m_CMD;								//待执行的命令
	pthread_t m_threadMemoryScript;				//执行内存脚本的线程
	char *m_MemoryScript;						//脚本字符串
	bool m_bIsMemoryScriptCancel;				//取消执行内存脚本
	bool m_bIsMemoryScriptFinish;				//脚本是否执行完成
	ULONG m_MemoryScriptLength;					//本次脚本长度
	
	void Connect();
	bool GetIsConnection();
	bool SendData(unsigned int type, const unsigned char* data, unsigned int length, int secondsTimeout);	
	bool SendData(unsigned int type, int secondsTimeout);
	
protected:
	virtual void OnReceived(unsigned int type, const unsigned char* data, unsigned int length);
	virtual void OnServerClose();
	virtual void OnSocketError();
private:
	pthread_mutex_t	_ServerSend_obj;
};

