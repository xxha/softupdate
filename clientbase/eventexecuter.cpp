/*****************************************************************************************
** Eventexecuter.h: Implementation of the event excuter class.
**
** Author: Leo.Tang
** Copyright (C) 2007 VeTronics(BeiJing) Ltd.
**
** Create date: 08-15-2007
*****************************************************************************************/
#include <pthread.h>
#include <stdio.h>

#include "clientbase.h"
#include "eventqueue.h"
#include "eventexecuter.h"

void* ExcuteThread(void* para)
{
	EventExecuter*	executer = (EventExecuter*)para;
	bool				ret;
	
	while(executer->KeepExecuting())
	{
		executer->Wait();
		
		do
		{
			ret = executer->DoEvent();
		}while(ret);
	}	

	printf("EventExecuter: Exit execueing thread.\n");
	return null;
}

EventExecuter::EventExecuter(ClientBase* client)
{
	pthread_mutex_init(&_cond_obj, null);
	pthread_mutex_init(&_keep_running_thread_obj, null);
	pthread_cond_init(&_cond, null);
	
	_client = client;
	_queue = new EventQueue_180(MAX_EVENTS_IN_QUEUE);

	_keep_running_thread = true;
	int ret = pthread_create(&(_execute_thread), null, ExcuteThread, this);
	if(0 != ret)
	{
		_execute_thread = null;
		printf("EventExecuter: Create execueing thread failed, error: %d.\n", ret);
	}
}

EventExecuter::~EventExecuter()
{
	//
	pthread_mutex_lock(&_cond_obj);
	if(0 != pthread_cond_broadcast(&_cond))
	{
		printf("EventExecuter: Error in resuming blocked executing thread.\n");
	}
	pthread_mutex_unlock(&_cond_obj);
	//
	SetKeepExecuting(false);
	//wait for thread exit
	if(null != _execute_thread)
	{
		pthread_join(_execute_thread, null);
	}
	
	pthread_mutex_destroy(&_cond_obj);
	pthread_mutex_destroy(&_keep_running_thread_obj);
	pthread_cond_destroy(&_cond);
	delete _queue;
}

void EventExecuter::Execute(const ReceivedData* data)
{
	bool ret = _queue->Enqueue(data);
	if(!ret)
	{
		printf("EventExecuter: Event queue is full.\n");
	}
	
	pthread_mutex_lock(&_cond_obj);
	if(0 != pthread_cond_broadcast(&_cond))
	{
		printf("EventExecuter: Error in resuming blocked executing thread.\n");
	}
	pthread_mutex_unlock(&_cond_obj);
}

bool EventExecuter::KeepExecuting()
{
	bool ret;
	
	pthread_mutex_lock(&_keep_running_thread_obj);
	ret = _keep_running_thread;
	pthread_mutex_unlock(&_keep_running_thread_obj);

	return ret;
}

void EventExecuter::SetKeepExecuting(bool value)
{
	pthread_mutex_lock(&_keep_running_thread_obj);
	_keep_running_thread = value;
	pthread_mutex_unlock(&_keep_running_thread_obj);
}

bool EventExecuter::DoEvent()
{
	ReceivedData* data;

	data = _queue->Dequeue(data);
	if(null != data)//check if there is any event in queue
	{
		_client->RaiseRecievedEvent(data->Type, data->Data, data->Length);

		delete data;
		return true;
	}

	return false;
}

void EventExecuter::Wait()
{
	struct timespec waittime;
	time_t now = time(null);
		
	if((time_t)(-1) == now)
	{
		return;
	}
		
	waittime.tv_sec = now + EXECUTE_WAIT_SECONDS;
  	waittime.tv_nsec = 0;
	
	pthread_mutex_lock(&_cond_obj);
	pthread_cond_timedwait(&_cond, &_cond_obj, &waittime);
	pthread_mutex_unlock(&_cond_obj);
}

