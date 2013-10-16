/*****************************************************************************************
** Client.h: Common data structure.
**
** Author: Leo.Tang
** Copyright (C) 2007 VeTronics(BeiJing) Ltd.
**
** Create date: 08-15-2007
*****************************************************************************************/
#ifndef common_class
#define common_class

#define	IN
#define	OUT
//const
#define	null						0
#define	KEEPALIVE_IDLE				1
#define	KEEPALIVE_INTERVAL			5
#define	KEEPALIVE_COUNT				3
#define	MAX_SIZE_OF_PACKET			32768 //lizhiyong not change.
#define	RECEIVE_BUFFER_SIZE			65536
#define	SEND_BUFFER_SIZE			65536
#define	APP_FLAG					"CX180"
#define	INVALID_SOCKET				(-1)
#define	SOCKET_ERROR				(-1)
#define	CONECT_TIMEOUT_SECONDS		5
#define	DISCONECT_TIMEOUT_SECONDS	5
#define	SEND_TIMEOUT_SECONDS			30
#define	MAX_EVENTS_IN_QUEUE			256
#define	EXECUTE_WAIT_SECONDS		5

#define	DISCONNECT_FAILED			0x01
#define	CREATE_THREAD_FAILED		0x02
#define	SET_SOCKET_OPT_FAILED		0x03

//message
#define	MSG_DATA_CORRECT	        0x00
#define	MSG_DATA_ERROR	            0x01
#define	MSG_CONNECT	            0x02

//network
typedef	int Socket;

//sizeof(FrameHeader) = 32bytes
//|Hash(16bytes)|AppFlag(8bytes)|DataLen(4bytes)|Type(2bytes)|
typedef struct st_FrameHeader
{
	unsigned char	MD5Hash[16];
	char		AppFlag[8];
	unsigned int	DataLength;
	unsigned int	Type;

	st_FrameHeader()
	{
		for(int i = 0; i < 16; i++)
		{
			MD5Hash[i] = 0;
		}
		for(int i = 0; i < 8; i++)
		{
			AppFlag[i] = 0;
		}
	}

} FrameHeader;

typedef struct st_ReceivedData
{
	unsigned int		Type;
	unsigned char*	    Data;
	unsigned int		Length;

	st_ReceivedData()
	{
		Data = null;
	}

	~st_ReceivedData()
	{
		if(null != Data)
		{
			delete []Data;
			Data=null;
		}
	}

} ReceivedData;

#endif  // common_class
