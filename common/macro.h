/*****************************************************************************************
** macro.h: 全局,预定义数据
**
** Author: cjiang
** Copyright (C) 2012 VeTronics(BeiJing) Ltd.
**
** Create date: 05-03-2012
*****************************************************************************************/
#pragma once
/*****************************************************************************/

#ifndef _MACRO_H_

#define _MACRO_H_

#ifndef ULONG
#define ULONG unsigned long
#endif

#ifndef USHORT
#define USHORT unsigned short
#endif

#ifndef UCHAR
#define UCHAR unsigned char
#endif

//20130508之前版本无版本号
//20130508:
//	1,添加版本控制，增加版本号，以日期表示[MSG_GETVERSION]
//	2,添加新的获取序列号命令[MSG_GETSERIALNUMBERALL]
//	3,添加新的获取文件信息命令[MSG_GETALLFILEINFO]
//  4,unit接收到未定义命令时,直接返回此命令,数据部分为空,可以避免版本升级以后,发送新命令到旧版,需要等待超时
//
#define VERSION 20130508							//软件版本号，以日期命名

#define  SOFTUPDATE_VERSION      1
#define  MAX_DIR_LEN            256  //The max directory length

#define MSG_GETCATALOG 0x100						//获取文件夹列表
#define MSG_CHECKFILEEXISTS 0x101					//检查文件是否存在
#define MSG_GETFILE 0x102							//获取文件
#define	MSG_GETFILE_CANCEL 0x103					//取消获取文件
#define	MSG_GETFILELENGTH 0x104						//获取文件长度
#define MSG_SENDFILE 0x105							//上传文件
#define	MSG_SENDFILE_CANCEL 0x106					//取消文件上传
#define	MSG_SENDFILENAME 0x107						//上传文件名称
#define MSG_GETFILECREATEDATE 0x108					//获取文件的创建时间
#define MSG_GETSERIALNUMBER 0x109					//获取本机的序列号[Obsolete],（此函数返回序列号全部为14为，而模块的序列号有可能有15位，所有废弃，使用新的命令：MSG_GETSERIALNUMBERALL）
#define MSG_SETSERIALNUMBER 0x110					//设置本机的序列号
#define MSG_GETMAC 0x111							//获取本机的MAC
#define MSG_SETMAC 0x112							//设置本机的MAC
#define MSG_READMEMORY 0x113						//读取内存
#define MSG_WRITEMEMORY 0x114						//写入内存
#define MSG_READMEMORY_CANCEL 0x115					//读取内存取消
#define MSG_CMD 0x116								//发送执行命令
#define MSG_CMD_CANCEL 0x117						//取消返回的结果
#define MSG_MEMORYSCRIPT 0x118						//内存脚本
#define MSG_MEMORYSCRIPT_START 0x119				//开始发送内存脚本
#define MSG_MEMORYSCRIPT_CANCEL 0x120				//取消发送内存脚本
#define MSG_MEMORYSCRIPT_FINISH 0x121				//内存脚本执行完成
#define MSG_GETBOARDTYPE 0x122						//获取模块类型
#define MSG_READMODULEBUF 0x123						//读模块
#define MSG_WRITEMODULEBUF 0x124					//写模块
#define MSG_WRITEOPTION 0x125			        	//写模块位              //开始地址:0X30
#define MSG_GETFILEINFO 0x126			        	//获得文件信息,创建时间,长度
////20130508版
#define MSG_GETSERIALNUMBERALL 0x127				//获取所有序列号，每个序列号直接使用"\r"间隔（2013-05-08，jiang,主板序列号长度为14为，其他模块序列号不等长）
#define MSG_GETVERSION 0x128						//获取软件版本号，以日期命名(20130508,jiang)
#define MSG_GETALLFILEINFO 0x129					//可以获取多个目录下的文件详细信息，(20130508,jiang,可以一次传多个目录路径，一次返回所有路径下文件的详细信息)

typedef unsigned int	UINT;
typedef unsigned char	BYTE;
typedef bool			BOOL;
typedef void*			LPVOID;
typedef char*			LPCHAR;

#define	NR_MAXPACKLEN			2048
#define MAX_MEM_LEN				256
#define MAX_FILENAME_LEN		256				//路径最大长度

#define SERVER_TCP_PORT			11000			//服务端TCP监听端口
#define SERVER_UDP_PORT			11100			//服务端UDP监听端口
#define LENGTH_OF_LISTEN_QUEUE	20 
#define BUFFER_SIZE				1024
#define  E2PROM_OPTION_SIZE     256
#define V400PANEL_BOARDNUM      (6)

//Roman modify from 0x20000000 to 0x10000000 @2006-11-1
#define  MIN_PHY_ADDR           0xC0000000

//Error code
#define  NULL_POINTER_ERR       -101 //Invalid pointer argument(s) error
#define  OPEN_FILE_ERR          -102 //Open file error
#define  INVALID_VER_ERR        -103 //Invalid image version error
#define  INVALID_ARG_ERR        -104 //Invalid argument(s) error
#define  ALLOC_MEM_FAIL         -105 //Malloc failed.
#define  INVALID_PHY_ADD        -106 //Invalid physical address range
#define  WRITE_FILE_ERR		    -107 //Writting length not equal the file length
#define  EXEC_SHELL_ERR			-108 //Call system fail
#define  IMAGE_MOUNT_ERR		-109
#define  IMAGE_GETINFO_ERR		-110
#define  IMAGE_WRITE_ERR		-111
#define  IMAGE_ERASE_ERR		-112
#define  IMAGE_COPY_ERR			-113
#define  IMAGE_UNMOUNT_ERR   	-114
#define  IMAGE_DELFILE_ERR   	-115
#define  CHECKSUM_NOT_EQUAL     -116 //Image checksum is not equal

#define MEM_READ_SUCS	        1	 //Read memory success
#define MEM_READ_FAIL	        2	 //Read memory fail
#define MEM_WRITE_SUCS	        3	 //Write memory success
#define MEM_WRITE_FAIL	        4	 //Write memory fail
#define MEM_OP_NOT_FINISH       5	 //memory operation isn't finish
#define MEM_INVALID_DATA	    6	 //Invalid memory data

#define MILLION  1000000

#define SIZE_EEPRom 4096
#define EEPROM_MAXSIZE      255

#define EEPRomStart 0x30

#define IndexValid(index)   (((0 <= index) && (index <= 5))?1:0)		//判断索引是否有效,范围:0 - 5

#define NK_SVR_TIMETOCHECK  2

typedef struct
{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
}TIME,* PTIME;

//文件详细信息
typedef struct
{
	UCHAR name[256];				//短文件名，不带路径
	UCHAR type[256];				//文件类型，传过来的参数，直接赋值即可。(当前路径下文件类型GETPATHFILEINFO中type)
	TIME time;
	int length;
}FILEINFODETAIL,*PFILEINFODETAIL;

#define FILEINFODETAIL_LEN (sizeof(FILEINFODETAIL))			//文件详细信息结构体长度

//获取此路径下文件详细信息
typedef struct
{
	UCHAR path[256];				//unit下文件夹路径
	UCHAR type[256];				//此路径下文件的类型，直接赋值给FILEINFODETAIL中type
	UCHAR filter[256];				//过滤条件，暂时未用到(20130508)
}GETPATHFILEINFO,*PGETPATHFILEINFO;

typedef struct
{
	TIME time;
	int length;
}FILEINFO,* PFILEINFO;

typedef struct
{
    int offset; //Option table
    UCHAR szPwd[9]; //User password
}OPTTABLEINFO300, *POPTTABLEINFO300;

typedef struct
{
	OPTTABLEINFO300 optTableInfo;
	int data;							//数据,0 or 1	
	int index;						//索引
}OPTIONINFO,* POPTIONINFO;

typedef struct
{
	int iStartAddr;			//开始地址
	int iLength;			//长度		
	int iIndex;				//索引
}MODULEINFO,*PMODULEINFO;

typedef struct
{
    ULONG ulStartAddr;
    ULONG ulLength;    
}MEMINFO, *PMEMINFO;

typedef struct
{
    MEMINFO stMemInfo;
    UCHAR *pucBuf; 
}MEMDATA, *PMEMDATA;

typedef enum eV400_board_type {
	eV400_board_unknown     = 0,

	eV400_board_40g,
	eV400_board_40ge,
	eV400_board_10g,
	eV400_board_2g5,
	eV400_board_1ge,
	eV400_board_100ge,
	eV400_board_16g,
} eV400_board_type;

extern bool StopAll;
extern int g_iServerTCPPort;
extern int g_iServerUDPPort;

bool StartCommonServer();				//启动服务
void StopCommonServer();				//停止服务
int DebugPrintf(char *format, ...);		//Debug输出
int ReadMAC(unsigned char *pMac);
int ReadSerialNumber(unsigned char *pSerialNumber);
int WriteMAC(unsigned char *pMac);
int WriteSerialNumber(unsigned char *pSerNo);
int WriteMemory(MEMDATA *pstMemData);
int ReadMemory(MEMDATA *pstMemData);
int ReadModuleBuf(PMODULEINFO pModuleInfo,unsigned char *pBuff);
int WriteModuleBuf(PMODULEINFO pModuleInfo,unsigned char *pBuff);
bool CheckPwd(OPTTABLEINFO300 stOptTbl,int index);
int WriteOptionBit(POPTIONINFO pOptionInfo);
int GetBoardresType(int index);
int v400_boardres_ind2sn (int board_index, char *sn);
int ReadOptionBufferIndex(unsigned char *buff, int start,int iLen, int index);
int FindAllSN();

#endif
