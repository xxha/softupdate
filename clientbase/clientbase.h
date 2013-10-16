/*****************************************************************************************
** Client.h: Definition of the CX180 client.
**
** Author: Leo.Tang
** Copyright (C) 2007 VeTronics(BeiJing) Ltd.
**
** Create date: 08-13-2007
*****************************************************************************************/
#ifndef clientbase_class
#define clientbase_class

#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>

#include "clientcommon.h"
#include "md5checksum.h"

class EventExecuter;

void* ReceiveThread(void* para); 
void ProcessSIGPIPESignal(int signo);

class ClientBase
{
	public:
		ClientBase();
		virtual ~ClientBase();

    public:
       	// Socket hNewSocket is the new socket handle
       	// Return: 0--succeeded, others--error
        int SetSocket( Socket hNewSocket );
		
		bool Connect(const char* IPAddress, unsigned short port, const char* device);
		bool Disconnect();		

		bool SimpleSend(unsigned int type);
		bool Send(unsigned int type);
		bool Send(unsigned int type, int secondsTimeout);
		bool Send(unsigned int type, const unsigned char* data, unsigned int length);
		bool Send(unsigned int type, const unsigned char* data, unsigned int length, int secondsTimeout);		
		
		bool IsSendSucceeded();

		bool Receive(unsigned int type, unsigned char* data, unsigned int* length, int secondsTimeout);

		bool KeepReceiving();		//2012-05-09,cjiang,private -> public

		friend void* ReceiveThread(void* para);
		friend class EventExecuter;

	protected:
		virtual void OnReceived(unsigned int type, const unsigned char* data, unsigned int length);
		virtual void OnServerClose();
		virtual void OnSocketError();
		
	private:		
		int CoreSend(Socket socket, unsigned char* data,  int length, int flag);
			
		unsigned char* CreateFrame(const char* appFlag, const unsigned char* data, unsigned int length, unsigned int type, unsigned char* buffer);

		Socket GetSocket();
		
		bool DisconnectForcibly();
		
		void Reply(bool successful);		
		void DealReplyMessage(const unsigned char* md5Hash, bool isSucceeded);
		
		//bool KeepReceiving();			//2012-05-09,cjiang,private -> public

		void StopSendWaiting();
		void StopDisconnectWaiting();
		void StopConnectWaiting();
		
		void Execute(unsigned int type, const unsigned char* data, unsigned int length);
		void RaiseRecievedEvent(unsigned int type, const unsigned char* data, unsigned int length);
		void RaiseServerCloseEvent();
		void RaiseSocketErrorEvent();		

		bool GetIsConnecting();			
		void SetIsConnecting(bool value);

		void SetIsSendSucceeded(bool value);
		void SetKeepReceiving(bool value);

		unsigned char* GetLastSentBytes(unsigned char* data, unsigned int* length);
		void	SetLastSentBytes(unsigned char* data, unsigned int length);

		unsigned char* GetReplyHashcode(unsigned char* data);
		void	SetReplyHashcode(unsigned char* data);

		void InitReceiveDataAndType(unsigned int type);
		void SetReceivedDataAndType(unsigned char* data, unsigned int length);
		unsigned char* GetReceivedData(unsigned char* data, unsigned int* length);
		unsigned int GetReceiveType();
			
	private:
		EventExecuter*	_executer;
		
		unsigned char*	_reply_hashcode;
		unsigned char*	_last_sent_bytes;
		int 			_last_sent_length;
		time_t			_last_sent_time;
		bool 			_send_succeeded;
		Socket			_socket;
		bool				_isconnecting;		
		unsigned int		_received_type;
		unsigned char*	_received_data;
		unsigned int		_received_length;
		
		pthread_mutex_t	_last_sent_bytes_obj; //sync obj
		pthread_mutex_t	_send_succeeded_obj;
		pthread_mutex_t	_keep_running_thread_obj;
        pthread_mutex_t	_reply_hashcode_obj;
		pthread_mutex_t	_isconnecting_obj;
		pthread_mutex_t	_send_obj;
		pthread_mutex_t	_receive_obj;

		pthread_mutex_t	_cond_obj;
		pthread_mutex_t	_cond_exit_obj;	
		pthread_mutex_t	_cond_connect_obj;
		pthread_mutex_t	_cond_receive_obj;
		pthread_cond_t	_cond;
		pthread_cond_t	_cond_exit;
		pthread_cond_t	_cond_connect;
		pthread_cond_t	_cond_receive;

		bool				_keep_running_thread;
		pthread_t			_receive_thread;

};

#endif
