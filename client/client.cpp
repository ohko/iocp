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

#define COUNT_THREAD 10
#define COUNT_THREAD_COUNT 5
#define COUNT_SEND 100

class client :IOCPClient {
	IOCP *io;
	std::atomic<int> m_want;
	std::atomic<int> m_send;
	std::atomic<int> m_recv;

	void print() {
		printf("want:%d, send:%d, recv:%d\n", m_want._My_val, m_send._My_val, m_recv._My_val);
	}

public:
	client() {
		io = new IOCP(this, true);
		while (!io->StartClient((char*)"127.0.0.1", 8080)) {
			debug("[C]reconnect1...");
			Sleep(1000);
		}

		for (int i = 1; i <= COUNT_SEND; i++) {
			m_want += i;
		}

		for (int i = 1; i <= COUNT_SEND; i++) {
			char *buf = (char*)malloc(i);
			memcpy(buf, (char*)&i, i < 4 ? 1 : 4);
			m_send += io->Send(buf, i);
			free(buf);
		}
	}
	~client() {
		if (m_recv != m_want) Sleep(1000);
		print();
		io->Close();
		delete io;
	}
	virtual void onRecv(char *buf, int len) {
		m_recv += len;
	}
	virtual void onClose() {
		debug("[C]close");
	}
};

void worker(void *) {
	auto c = new client();
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
		Sleep(5000);
	}

	getchar();
	return 0;
}
