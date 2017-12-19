#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdlib.h>
#include <process.h>
#include <atomic>

#include "../iocp/iocp.h"

#ifdef _DEBUG
#pragma comment(lib,"../debug/iocp.lib")
#else
#pragma comment(lib,"../release/iocp.lib")
#endif

std::atomic<int> m_send;
std::atomic<int> m_recv;

#define COUNT_THREAD 10
#define COUNT_THREAD_COUNT 2
#define COUNT_SEND 10000

void print() {
	printf("send:%d, recv:%d\n", m_send._My_val, m_recv._My_val);
}

class client :IOCPClient {
	IOCP *io;
public:
	client() {
		io = new IOCP(this, true);
		while (!io->StartClient((char*)"127.0.0.1", 8080)) {
			debug("[C]reconnect1...");
			Sleep(1000);
		}

		char msg[0x10];
		for (int i = 1; i <= COUNT_SEND; i++) {
			sprintf_s(msg, "%d", i);
			io->Send(msg, strlen(msg));
			m_send += i;
		}
	}
	~client() {
		io->Close();
		delete io;
	}
	virtual void onRecv(char *buf, int len) {

		char *data = new char[len + 1];
		data[len] = 0;
		memcpy(data, buf, len);

		int i = atoi(data);
		m_recv += i;
		//debug("[C]reccv:%d %s %d", len, data, i);

		delete data;
	}
	virtual void onClose() {
		debug("[C]close");
	}
};

void worker(void *) {
	auto c = new client();
	Sleep(1000);
	delete c;
}

int main()
{
	while (1) {
		for (int i = 0; i < COUNT_THREAD; i++) {
			for (int j = 0; j < COUNT_THREAD_COUNT; j++) {
				_beginthread(worker, 0, 0);
			}
		}
		Sleep(10000);
		print();
	}

	getchar();
	return 0;
}
