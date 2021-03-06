#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdlib.h>
#include <atomic>

#include "../iocp/iocp.h"

class server :IOCPServer {
	IOCP *io;
	std::atomic<int> m_client;
	std::atomic<int> m_sum;
public:
	server() {
		IocpInitApis();
		io = iocpServer(this, true);
		if (io->StartServer(INADDR_ANY, 8080)) {
			debug("[S]server start");
		}
		else {
			debug("[S]server start error");
		}
	}
	~server() {
		delete io;
	}
	virtual void onNewClient(SOCKET s, char *ip, int port) {
		debug("[S]new client: %s:%d", ip, port);
		m_client++;
		print();
	}
	virtual void onRecv(SOCKET s, char *ip, int port, char *buf, int len) {
		int i = 0;
		memcpy((char*)&i, buf, len < 4 ? 1 : 4);
		if (i != len) {
			printf("i: %d, len:%d\n", i, len);
		}
		m_sum += len;
		io->Send(s, buf, len);
	}
	virtual void onClose(SOCKET s, char *ip, int port) {
		debug("[S]close client: %s:%d", ip, port);
		m_client--;
		print();
	}

	void print() {
		printf("client: %d, sum:%d\n", m_client._My_val, m_sum._My_val);
	}
};

int main()
{
	server *s = new server();
	getchar();
	delete s;
	getchar();
	return 0;
}

