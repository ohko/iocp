#include "IOCP.h"
#include <process.h>
#include <ws2tcpip.h>

#pragma comment(lib,"ws2_32")

typedef struct IOCP_CONTEXT {
	WSAOVERLAPPED overlapped;
	SOCKET s;
	WSABUF wsaBuf;
	DWORD flags;
	char ip[16];
	int port;
} *LPPER_IO_DATA;

#define MAX_CLIENT_COUNT 0x100
#define DEBAULT_BUFFSIZE 0x20

IOCP::IOCP(IOCPClient *clientClass, bool packed) {
	m_s = 0;
	m_hIocp = 0;
	m_clsClient = clientClass;
	m_packed = packed;
}
IOCP::IOCP(IOCPServer *serverClass, bool packed) {
	m_s = 0;
	m_hIocp = 0;
	m_clsServer = serverClass;
	m_packed = packed;
}


IOCP::~IOCP() {
	if (m_s) { closesocket(m_s); m_s = 0; }
	m_lock.lock();
	for (auto it = m_tmpBuf.begin(); it != m_tmpBuf.end(); it++) closesocket(it->first);
	m_lock.unlock();
	PostQueuedCompletionStatus(m_hIocp, 0, 0, 0);
	while (!m_canRelesase) Sleep(10);
	if (m_hIocp) { CloseHandle(m_hIocp); m_hIocp = 0; }
}


bool IOCP::StartServer(char *host, int port) {
	WSADATA WsaDat;
	if (WSAStartup(MAKEWORD(2, 2), &WsaDat) != 0) return false;

	if ((m_s = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		m_s = 0;
		return false;
	}

	SOCKADDR_IN addr;
	int addrLen = sizeof(addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (host) inet_pton(AF_INET, host, &addr.sin_addr.s_addr);
	else addr.sin_addr.S_un.S_addr = INADDR_ANY;

	// bind
	if (bind(m_s, (sockaddr*)(&addr), addrLen) == SOCKET_ERROR) {
		closesocket(m_s); m_s = 0;
		return false;
	}

	// listen
	if (listen(m_s, SOMAXCONN) == SOCKET_ERROR) {
		closesocket(m_s); m_s = 0;
		return false;
	}

	if ((m_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) == NULL) {
		closesocket(m_s); m_s = 0;
		return false;
	}
	if (CreateIoCompletionPort((HANDLE)m_s, m_hIocp, 0, 0) == NULL) {
		closesocket(m_s); m_s = 0;
		CloseHandle(m_hIocp); m_hIocp = 0;
		return false;
	}

	m_canRelesase = false;
	_beginthread(IOCP::threadServer, 0, this);
	_beginthread(IOCP::threadAccept, 0, this);
	return true;
}

void IOCP::threadAccept(void *param) {
	IOCP *cls = (IOCP*)param;
	SOCKADDR_IN addr;
	int addrLen = sizeof(addr);

	debug("[S]accept thread start");
	while (1) {
		SOCKET c = WSAAccept(cls->m_s, (sockaddr*)&addr, &addrLen, NULL, NULL);
		if (c == INVALID_SOCKET) break;

		if (CreateIoCompletionPort((HANDLE)c, cls->m_hIocp, c, 0) == NULL) {
			closesocket(c);
			continue;
		}

		IOCP_CONTEXT *ctx = new IOCP_CONTEXT;
		ZeroMemory(ctx, sizeof(IOCP_CONTEXT));
		ctx->s = c;
		ctx->wsaBuf.buf = new char[DEBAULT_BUFFSIZE];
		ctx->wsaBuf.len = DEBAULT_BUFFSIZE;
		ctx->port = addr.sin_port;
		inet_ntop(addr.sin_family, &addr, ctx->ip, sizeof(ctx->ip));

		DWORD dwNumRecv;
		if (WSARecv(ctx->s, &ctx->wsaBuf, 1, &dwNumRecv, &ctx->flags, &ctx->overlapped, NULL) == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
			closesocket(c); c = 0;
			delete[] ctx->wsaBuf.buf;
			delete ctx;
			continue;
		}
		cls->m_lock.lock();
		cls->m_tmpBuf[c];
		cls->m_lock.unlock();
		cls->m_clsServer->onNewClient(c, ctx->ip, ctx->port);
	}
	debug("[S]accept thread end");
}


void IOCP::threadServer(void *param) {
	IOCP *cls = (IOCP*)param;
	DWORD NumBytesRecv = 0;
	ULONG CompletionKey;
	LPPER_IO_DATA p_ctx;


	debug("[S]iocp thread start");
	while (1) {
		BOOL r = GetQueuedCompletionStatus(cls->m_hIocp, &NumBytesRecv, &CompletionKey, (LPOVERLAPPED*)&p_ctx, INFINITE);
		//debug("r=%x, NumBytesRecv=%d, p_ctx=0x%08x, socket=0x%x", r, NumBytesRecv, p_ctx, (p_ctx ? p_ctx->s : 0));

		// exit
		if (r == TRUE && NumBytesRecv == 0 && p_ctx == 0) break;

		// client exit
		if (r == FALSE && p_ctx != 0) {
			cls->m_clsServer->onClose(p_ctx->s, p_ctx->ip, p_ctx->port);
			cls->m_lock.lock();
			cls->m_tmpBuf[p_ctx->s].shrink_to_fit();
			cls->m_tmpBuf.erase(p_ctx->s);
			cls->m_lock.unlock();
			delete[] p_ctx->wsaBuf.buf;
			delete p_ctx;
			continue;
		}

		if (NumBytesRecv == 0) continue;

		// callback
		if (!cls->m_packed) cls->m_clsServer->onRecv(p_ctx->s, p_ctx->ip, p_ctx->port, p_ctx->wsaBuf.buf, NumBytesRecv);
		else {
			std::string *tmp = &cls->m_tmpBuf[p_ctx->s];
			tmp->append(p_ctx->wsaBuf.buf, NumBytesRecv);
			if (tmp->length() > 4) {
				unsigned int needLen = *(int*)tmp->c_str();
				while (needLen <= tmp->length()) {
					//debug("%d:%c", *(int*)tmpBuf.c_str(), *(char*)(tmpBuf.c_str() + 4));
					cls->m_clsServer->onRecv(p_ctx->s, p_ctx->ip, p_ctx->port, (char*)tmp->c_str() + 4, needLen - 4);
					tmp->erase(0, needLen);
					if (tmp->length() <= 4) break;
					needLen = *(int*)tmp->c_str();
				}
			}
		}

		// next
		DWORD dwNumRecv;
		if (WSARecv(p_ctx->s, &p_ctx->wsaBuf, 1, &dwNumRecv, &p_ctx->flags, &p_ctx->overlapped, NULL) == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
			cls->m_clsServer->onClose(p_ctx->s, p_ctx->ip, p_ctx->port);
			delete[] p_ctx->wsaBuf.buf;
			delete p_ctx;
			continue;
		}
	}
	debug("[S]iocp thread end");
	cls->m_canRelesase = true;
}


bool IOCP::StartClient(char *host, int port) {
	WSADATA WsaDat;
	if (WSAStartup(MAKEWORD(2, 2), &WsaDat) != 0) return false;

	m_s = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_s == INVALID_SOCKET) {
		m_s = 0;
		return false;
	}

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, host, &addr.sin_addr.s_addr);
	addr.sin_port = htons(port);

	m_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	CreateIoCompletionPort((HANDLE)m_s, m_hIocp, 0, 0);

	// connect
	if (WSAConnect(m_s, (SOCKADDR*)(&addr), sizeof(addr), NULL, NULL, NULL, NULL) == SOCKET_ERROR) {
		closesocket(m_s); m_s = 0;
		if (m_hIocp != 0) { CloseHandle(m_hIocp); m_hIocp = 0; }
		return false;
	}

	m_canRelesase = false;
	_beginthread(IOCP::threadClient, 0, this);

	IOCP_CONTEXT *m_ctx = new IOCP_CONTEXT;
	ZeroMemory(m_ctx, sizeof(IOCP_CONTEXT));
	m_ctx->s = m_s;
	m_ctx->wsaBuf.buf = new char[DEBAULT_BUFFSIZE];
	m_ctx->wsaBuf.len = DEBAULT_BUFFSIZE;

	DWORD dwNumRecv;
	if (WSARecv(m_ctx->s, &m_ctx->wsaBuf, 1, &dwNumRecv, &m_ctx->flags, &m_ctx->overlapped, NULL) == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		closesocket(m_s); m_s = 0;
		if (m_hIocp != 0) { CloseHandle(m_hIocp); m_hIocp = 0; }
		delete[] m_ctx->wsaBuf.buf;
		delete m_ctx;
		return false;
	}

	return true;
}

void IOCP::threadClient(void *param) {
	IOCP *cls = (IOCP*)param;
	DWORD NumBytesRecv = 0;
	ULONG CompletionKey;
	LPPER_IO_DATA p_ctx;

	std::string tmpBuf;

	debug("[C]iocp thread start");
	while (1)
	{
		BOOL r = GetQueuedCompletionStatus(cls->m_hIocp, &NumBytesRecv, &CompletionKey, (LPOVERLAPPED*)&p_ctx, INFINITE);
		//debug("r=%x, NumBytesRecv=%d, p_ctx=0x%08x, socket=0x%x", r, NumBytesRecv, p_ctx, (p_ctx ? p_ctx->s : 0));

		// exit
		if (r == TRUE && NumBytesRecv == 0 && p_ctx == 0) break;

		if (r == FALSE && p_ctx != 0) {
			delete[] p_ctx->wsaBuf.buf;
			delete p_ctx;
			break;
		}

		if (NumBytesRecv == 0) continue;

		// callback
		if (!cls->m_packed) cls->m_clsClient->onRecv(p_ctx->wsaBuf.buf, NumBytesRecv);
		else {
			tmpBuf.append(p_ctx->wsaBuf.buf, NumBytesRecv);
			if (tmpBuf.length() > 4) {
				unsigned int needLen = (*(int*)tmpBuf.c_str());
				while (needLen <= tmpBuf.length()) {
					//debug("%d:%c", *(int*)tmpBuf.c_str(), *(char*)(tmpBuf.c_str() + 4));
					cls->m_clsClient->onRecv((char*)tmpBuf.c_str() + 4, needLen - 4);
					tmpBuf.erase(0, needLen);
					if (tmpBuf.length() > 4) needLen = *(int*)tmpBuf.c_str();
					else break;
				}
			}
		}

		// next
		DWORD dwNumRecv;
		if (WSARecv(p_ctx->s, &p_ctx->wsaBuf, 1, &dwNumRecv, &p_ctx->flags, &p_ctx->overlapped, NULL) == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
			delete[] p_ctx->wsaBuf.buf;
			delete p_ctx;
			break;
		}
	}
	debug("[C]iocp thread end");
	cls->m_clsClient->onClose();
	cls->m_canRelesase = true;
}


int IOCP::Send(char* buffer, int len) {
	return Send(m_s, buffer, len);
}

int IOCP::Send(SOCKET s, char* buffer, int len) {
	if (!m_packed) return send(s, buffer, len, 0);
	else {
		char *pack = new char[len + 4];
		*(int*)pack = len + 4;
		memcpy(pack + 4, buffer, len);
		int i = send(s, pack, len + 4, 0);
		delete[] pack;
		return i - 4;
	}
}

void IOCP::Close() {
	if (m_s) { closesocket(m_s); m_s = 0; }
}
