/*****************************************************************************************
** SingleService.h: 用来与远程建立连接的客户端
** 负责与远程连接进行交互
** Author: cjiang
** Copyright (C) 2012 VeTronics(BeiJing) Ltd.
**
** Create date: 05-03-2012
*****************************************************************************************/

#pragma once

class CSingleService
{
public:
	CSingleService(void);
	~CSingleService(void);

	bool CreateService();
	bool CloseService();
};

