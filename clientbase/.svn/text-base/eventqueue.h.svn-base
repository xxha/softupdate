/*****************************************************************************************
** Eventqueue.h: Definition of the queue class.
**
** Author: Leo.Tang
** Copyright (C) 2007 VeTronics(BeiJing) Ltd.
**
** Create date: 08-15-2007
*****************************************************************************************/
#ifndef eventqueue_class
#define eventqueue_class

#include <pthread.h>
#include <string.h>
#include "clientcommon.h"

class DataNode
{
	private:
		ReceivedData* _data;
	
	public:
		DataNode* Next;
		DataNode* Pre;
		
		DataNode(const ReceivedData* data, DataNode* next, DataNode* pre)
		{
			_data = new ReceivedData;

			if(null != data)
			{
				_data->Type = data->Type;
				_data->Length= data->Length;
				if((null != data->Data) && (data->Length > 0))
				{
					_data->Data = new unsigned char[data->Length];
					memcpy(_data->Data, data->Data, data->Length);
				}
			}
			
			Next = next;
			Pre = pre;
		}

		~DataNode()
		{
			if(null != _data)
			{
				delete _data;
			}
		}
		
		ReceivedData* GetData(ReceivedData* data)
		{
			data = new ReceivedData;
			
			if(null != _data)
			{
				data->Type = _data->Type;
				data->Length = _data->Length;
				if((null != _data->Data) && (_data->Length > 0))
				{
					data->Data = new unsigned char[_data->Length];
					memcpy(data->Data, _data->Data, _data->Length);
				}
			}

			return data;
		}
};

class EventQueue_180
{
	public:
		EventQueue_180(int size = 256);
		~EventQueue_180();
		
		bool Enqueue(const ReceivedData* data);
		ReceivedData* Dequeue(ReceivedData* data);

		int GetCount();
		
	private:
		int _maxcount;
		int _count;
		DataNode* _first;
		DataNode* _last;

		pthread_mutex_t	_obj; //sync obj
};

#endif

