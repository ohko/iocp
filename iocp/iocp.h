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
public:
	virtual void onRecv(char *buf, int len) = 0;
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
	IOCP(IOCPServer *serverClass, bool packed); // ����˶���
	IOCP(IOCPClient *clientClass, bool packed); // �ͻ��˶���
	virtual ~IOCP(); // �ͷ�
	virtual bool StartServer(char *host, int port); // ����������
	virtual bool StartClient(char *host, int port); // �����ͻ���
	virtual int Send(char* buffer, int len); // �������� 
	virtual int Send(SOCKET s, char* buffer, int len); // �������� 
	virtual void Close(); // �رտͻ��˻������
private:
	bool m_canRelesase; // �Ƿ�����ͷŶ�����
	SOCKET m_s; // ��socket
	HANDLE m_hIocp; // io���
	bool m_packed;

	std::map<SOCKET, std::string> m_tmpBuf; // ���տͻ������ݻ���
	std::mutex m_lock;
	IOCPServer *m_clsServer; // �ص������
	static void threadAccept(void *param); // ���ܿͻ����߳�
	static void threadServer(void *param); // ���ܿͻ��������߳�

	IOCPClient *m_clsClient; // �ص��ͻ���
	static void threadClient(void *param); // ����˽��������߳�
};

