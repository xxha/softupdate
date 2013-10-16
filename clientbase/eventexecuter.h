/*****************************************************************************************
** Eventexecuter.h: Definition of the event excuter class.
**
** Author: Leo.Tang
** Copyright (C) 2007 VeTronics(BeiJing) Ltd.
**
** Create date: 08-15-2007
*****************************************************************************************/
#ifndef eventexcuter_class
#define eventexcuter_class

#include <pthread.h>

#include "clientcommon.h"

void* ExcuteThread(void* para);

class EventQueue_180;
class ClientBase;

class EventExecuter
{
	public:
		EventExecuter(ClientBase* client);
		~EventExecuter();

		void Execute(const ReceivedData* data);

		friend void* ExcuteThread(void* para);
		
	private:			
		void Wait();		
		bool DoEvent();
		bool KeepExecuting();
		void SetKeepExecuting(bool value);	
		
	private:		
		pthread_mutex_t	_cond_obj;
		pthread_mutex_t	_keep_running_thread_obj;
		pthread_cond_t	_cond;
		bool				_keep_running_thread;
		pthread_t			_execute_thread;

		ClientBase*			_client;
		EventQueue_180*		_queue;
};

#endif // eventexcuter_class

