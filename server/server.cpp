#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdlib.h>
#include <atomic>

#include "../iocp/iocp.h"

#ifdef _DEBUG
#pragma comment(lib,"../debug/iocp.lib")
#else
#pragma comment(lib,"../release/iocp.lib")
#endif

class server :IOCPServer {
	IOCP *io;
	std::atomic<int> m_client;
	std::atomic<int> m_sum;
public:
	server() {
		io = new IOCP(this, true);
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
		//debug("[S]new client: %s:%d", ip, port);
		m_client++;
		print();
	}
	virtual void onRecv(SOCKET s, char *ip, int port, char *buf, int len) {
		char *data = new char[len+1];
		data[len] = 0;
		memcpy(data, buf, len);
		int n = atoi(data);
		m_sum += n;
		io->Send(s, buf, len);
		delete[] data;
	}
	virtual void onClose(SOCKET s, char *ip, int port) {
		//debug("[S]close client: %s:%d", ip, port);
		m_client--;
		print();
	}

	void print() {
		printf("client: %d, sum:%d\n", m_client._My_val, m_sum._My_val);
	}
};

int main()
{
	//getchar();
	server *s = new server();
	getchar();
	delete s;
	getchar();
	return 0;
}

