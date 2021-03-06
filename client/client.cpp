#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdlib.h>
#include <process.h>
#include <atomic>

#include "../iocp/iocp.h"

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
		IocpInitApis();
		io = iocpClient(this, true);
		while (!io->StartAsyncClient((char*)"127.0.0.1", 8080)) {
			debug("[C]reconnect1...");
			Sleep(1000);
		}

		for (int i = 1; i <= COUNT_SEND; i++) {
			m_want += i;
		}

		for (int i = 1; i <= COUNT_SEND; i++) {
			char *buf = (char*)malloc(i);
			memcpy(buf, (char*)&i, i < 4 ? 1 : 4);
			m_send += io->SendAsync(buf, i);
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

void syncSend(void *) {
	IocpInitApis();
	IOCP *io = iocpClient(0, true);
	while (!io->StartSyncClient((char*)"127.0.0.1", 8080)) {
		debug("[C]reconnect1...");
		Sleep(1000);
	}
	char *dst = 0;
	char bSend[] = "Hello";
	int dstLen = io->SendSync(&dst, bSend, sizeof(bSend));
	if (dst) free(dst);
	printf("===== Sync Send: %d, Recv: %d =====\n", sizeof(bSend), dstLen);
}

int main()
{
	while (1) {
		for (int i = 0; i < COUNT_THREAD; i++) {
			for (int j = 0; j < COUNT_THREAD_COUNT; j++) {
				_beginthread(worker, 0, 0);
				_beginthread(syncSend, 0, 0);
			}
		}
		Sleep(5000);
	}

	getchar();
	return 0;
}
