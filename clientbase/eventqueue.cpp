/*****************************************************************************************
** Eventqueue.cpp: Implementation of the queue class.
**
** Author: Leo.Tang
** Copyright (C) 2007 VeTronics(BeiJing) Ltd.
**
** Create date: 04-28-2007
*****************************************************************************************/
#include <stdio.h>
#include <pthread.h>
#include "eventqueue.h"

EventQueue_180::~EventQueue_180()
{
	pthread_mutex_lock(&_obj);
	DataNode* tmp = _first;
	
	if(null != tmp)
	{
		while(null != tmp->Next)
		{
			tmp = tmp->Next;
			delete tmp->Pre;
		}
		delete tmp;
	}
	pthread_mutex_unlock(&_obj);

	pthread_mutex_destroy(&_obj);
}

EventQueue_180::EventQueue_180(int size)
{
	pthread_mutex_init(&_obj, null);
	
	_maxcount = size;
	_count = 0;
	_first = _last = null;
}

bool EventQueue_180::Enqueue(const ReceivedData* data)
{
	pthread_mutex_lock(&_obj);
	if(_count >= _maxcount)
	{
		pthread_mutex_unlock(&_obj);
		return false;
	}

	DataNode* tmp;
	if(_count == 0)//first node
	{		
		tmp = new DataNode(data, null, null);
		
		_first = tmp;
		_last = tmp;
	}
	else
	{
		tmp = new DataNode(data, null, _last);
		
		_last->Next = tmp;		
		_last = tmp;
	}

	_count++;
	pthread_mutex_unlock(&_obj);
	
	return true;
}

ReceivedData* EventQueue_180::Dequeue(ReceivedData* data)
{
	pthread_mutex_lock(&_obj);
	if(_count <= 0)
	{
		pthread_mutex_unlock(&_obj);
		return null;
	}
	
	DataNode* tmp = _first;
	
	data = tmp->GetData(data);

	_first = tmp->Next;
	if(null != _first)
	{
		_first->Pre = null;
	}
	
	delete tmp;
	_count--;	
	pthread_mutex_unlock(&_obj);
	
	return data;
}

int EventQueue_180::GetCount()
{
	int count;
	pthread_mutex_lock(&_obj);
	count = _count;
	pthread_mutex_unlock(&_obj);
	
	return count;
}
	
