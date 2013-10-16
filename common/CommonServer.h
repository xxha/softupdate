
/*****************************************************************************************
** CommonServer.h: ������Զ�̽������ӵĿͻ���
** ������Զ�����ӽ��н���
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

	bool m_bIsGetFileCancel;					//�ļ�����ȡ���ı��
	bool m_bIsSendFileCancel;					//�ļ��ϴ�ȡ���ı��
	FILE *m_fpSendFile;							//�ϴ��ļ�ʱ�����ļ����
	char *m_pSendFilePath;						//�ϴ��ļ�ʱ����·��
	char *m_pGetFilePath;						//�����ļ�ʱ����·��
	pthread_t m_threadDownFile;					//�����ļ����߳�
	pthread_t m_threadReadMemory;				//��ȡ�ڴ���߳�
	MEMINFO m_memInfoRead;						//��ȡ�ڴ�ʱ�Ĳ���
	bool m_bIsReadMemoryCancel;					//��ȡ�ڴ�ȡ���ı��
	bool m_bIsCMDCancel;						//ִ�������ķ���ֵȡ���ı��
	pthread_t m_threadCMD;						//ִ�������ķ���ֵ
	char *m_CMD;								//��ִ�е�����
	pthread_t m_threadMemoryScript;				//ִ���ڴ�ű����߳�
	char *m_MemoryScript;						//�ű��ַ���
	bool m_bIsMemoryScriptCancel;				//ȡ��ִ���ڴ�ű�
	bool m_bIsMemoryScriptFinish;				//�ű��Ƿ�ִ�����
	ULONG m_MemoryScriptLength;					//���νű�����
	
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

