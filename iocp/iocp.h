#pragma once
#include <WinSock2.h>
#include <vector>
#include <mutex>
#include <map>

#ifndef debug
#ifdef _DEBUG
#include <stdio.h>
#include <time.h>
#define debug(format,...){time_t tt = time(NULL);tm t; localtime_s(&t,&tt);printf_s("[%02d:%02d:%02d] "##format##" (%s:%d)\n",t.tm_hour,t.tm_min,t.tm_sec,##__VA_ARGS__,__FILE__,__LINE__);}
#else
#define debug(format,...)void()
#endif
#endif // !debug

class IOCPClient {
	std::mutex m_lock; // 同步发送数据使用,同一时间只能一个发送和接收
public:
	virtual void onRecv(char *buf, int len) {};
	virtual void onClose() {};
};
class IOCPServer {
public:
	virtual void onNewClient(SOCKET s, char *ip, int port) {};
	virtual void onRecv(SOCKET s, char *ip, int port, char *buf, int len) = 0;
	virtual void onClose(SOCKET s, char *ip, int port) {};
};

class IOCP
{
public:
	IOCP(IOCPServer *serverClass, bool packed); // 服务端对象
	IOCP(IOCPClient *clientClass, bool packed); // 客户端对象
	virtual ~IOCP(); // 释放
	virtual bool StartServer(char *host, int port); // 启动服务器
	virtual bool StartSyncClient(char *host, int port); // 启动同步客户端
	virtual bool StartAsyncClient(char *host, int port); // 启动异步客户端
	virtual int SendSync(char **needFreeDst, char* buffer, int len); // client发送同步数据,needFreeDst需要调用端用free释放内存
	virtual int SendAsync(char* buffer, int len); // client发送异步数据
	virtual int Send(SOCKET s, char* buffer, int len); // 发送数据 
	virtual void Close(); // 关闭客户端或服务器
	virtual void Free(char* buffer); // 释放malloc申请的内存
private:
	bool m_canRelesase; // 是否可以释放对象了
	SOCKET m_s; // 主socket
	HANDLE m_hIocp; // io句柄
	bool m_packed;

	std::map<SOCKET, std::string> m_tmpBuf; // 接收客户端数据缓冲
	std::mutex m_lock;
	IOCPServer *m_clsServer; // 回调服务端
	static void threadAccept(void *param); // 接受客户端线程
	static void threadServer(void *param); // 接受客户端数据线程

	IOCPClient *m_clsClient; // 回调客户端
	static void threadClient(void *param); // 服务端接受数据线程
};

typedef IOCP* (*IocpServer)(IOCPServer *serverClass, bool packed);
typedef IOCP* (*IocpClient)(IOCPClient *clientClass, bool packed);
#define IocpInitApis()\
HMODULE hDll = LoadLibraryA("iocp.dll");\
IocpServer iocpServer;IocpClient iocpClient;\
if(!hDll){MessageBoxA(0,"Can't load iocp.dll!","Error",0);}\
iocpServer = (IocpServer)GetProcAddress(hDll, "getServerInstance");\
iocpClient = (IocpClient)GetProcAddress(hDll, "getClientInstance");