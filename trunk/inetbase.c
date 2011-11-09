/**********************************************************************
 *
 * inetbase.c - basic interface of socket operation
 *
 * for more information, please see the readme file
 *
 **********************************************************************/

#include "inetbase.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#ifdef __unix
#include <netdb.h>
#include <sched.h>
#include <pthread.h>
#include <poll.h>
#include <dlfcn.h>
#include <signal.h>

#elif (defined(_WIN32) || defined(WIN32))
#if ((!defined(_M_PPC)) && (!defined(_M_PPC_BE)) && (!defined(_XBOX)))
#include <mmsystem.h>
#include <mswsock.h>
#include <process.h>
#include <stddef.h>
#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable:4312)
#pragma warning(disable:4996)
#endif
#else
#include <process.h>
#pragma comment(lib, "xnet.lib")
#endif
#endif


/*===================================================================*/
/* Internal Macro Definition                                         */
/*===================================================================*/
#if defined(TCP_CORK) && !defined(TCP_NOPUSH)
#define TCP_NOPUSH TCP_CORK
#endif

#ifdef __unix
typedef socklen_t DSOCKLEN_T;
#else
typedef int DSOCKLEN_T;
#endif



/*===================================================================*/
/* Cross-Platform Time Interface                                     */
/*===================================================================*/

/* global millisecond clock value, updated by itimeofday */
volatile IINT64 itimeclock = 0;	

/* default mode = 0, using timeGetTime in win32 instead of QPC */
int itimemode = 0;


/*-------------------------------------------------------------------*/
/* sleep in millisecond                                              */
/*-------------------------------------------------------------------*/
void isleep(unsigned long millisecond)
{
	#ifdef __unix 	/* usleep( time * 1000 ); */
	struct timespec ts;
	ts.tv_sec = (time_t)(millisecond / 1000);
	ts.tv_nsec = (long)((millisecond % 1000) * 1000000);
	//nanosleep(&ts, NULL);
	usleep((millisecond << 10) - (millisecond << 4) - (millisecond << 3));
	#elif defined(_WIN32)
	Sleep(millisecond);
	#endif
}

/*-------------------------------------------------------------------*/
/* get system time                                                   */
/*-------------------------------------------------------------------*/
static void itimeofday_default(long *sec, long *usec)
{
	#if defined(__unix)
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
	#elif defined(_WIN32)
	static long mode = 0, addsec = 0;
	BOOL retval;
	static IINT64 freq = 1;
	IINT64 qpc;
	if (mode == 0) {
		retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		freq = (freq == 0)? 1 : freq;
		retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
		addsec = (long)time(NULL);
		addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
		mode = 1;
	}
	retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
	retval = retval * 2;
	if (sec) *sec = (long)(qpc / freq) + addsec;
	if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
	#endif
}

static void itimeofday_clock(long *sec, long *usec)
{
	#if defined(_WIN32) && !defined(_XBOX)
	static unsigned long mode = 0, addsec;
	static unsigned long lasttick = 0;
	static IINT64 hitime = 0;
	static CRITICAL_SECTION mutex;
	unsigned long _cvalue;
	IINT64 current;
	if (mode == 0) {
		lasttick = timeGetTime();
		addsec = (unsigned long)time(NULL);
		addsec = addsec - lasttick / 1000;
		InitializeCriticalSection(&mutex);
		mode = 1;
	}
	_cvalue = timeGetTime();
	EnterCriticalSection(&mutex);
	if (_cvalue < lasttick) {
		hitime += 0x80000000ul;
		lasttick = _cvalue;
		hitime += 0x80000000ul;
	}
	LeaveCriticalSection(&mutex);
	current = hitime | _cvalue;
	if (sec) *sec = (long)(current / 1000) + addsec;
	if (usec) *usec = (long)(current % 1000) * 1000;
	#else
	itimeofday_default(sec, usec);
	#endif
}


/*-------------------------------------------------------------------*/
/* get time of day                                                   */
/*-------------------------------------------------------------------*/
void itimeofday(long *sec, long *usec)
{
	IINT64 value;
	long s, u;
	if (itimemode == 0) {
		itimeofday_clock(&s, &u);
	}	else {
		itimeofday_default(&s, &u);
	}
	value = ((IINT64)s) * 1000 + (u / 1000);
	itimeclock = value;
	if (sec) *sec = s;
	if (usec) *usec = u;
}


/*-------------------------------------------------------------------*/
/* get clock in millisecond 64                                       */
/*-------------------------------------------------------------------*/
IINT64 iclock64(void)
{
	IINT64 value;
	itimeofday(NULL, NULL);
	value = itimeclock;
	return value;
}

/*-------------------------------------------------------------------*/
/* get clock in millisecond                                          */
/*-------------------------------------------------------------------*/
unsigned long iclock(void)
{
	iclock64();
	return (unsigned long)(itimeclock & 0xfffffffful);
}


/*===================================================================*/
/* Cross-Platform Threading Interface                                */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/* create thread                                                     */
/*-------------------------------------------------------------------*/
int ithread_create(ilong *id, ITHREADPROC fun, const void *attr, void *args)
{
	#ifdef __unix
	pthread_t newthread;
	long ret = pthread_create((pthread_t*)&newthread, NULL, 
		(void*(*)(void*)) fun, args);
	if (id) *id = (long)newthread;
	if (ret) return -1;
	#elif defined(_WIN32)
	ilong Handle;
	Handle = (ilong)_beginthread((void(*)(void*))fun, 0, args);
	if (id) *id = (ilong)Handle;
	if (Handle == 0) return -1;
	#endif
	return 0;
}

/*-------------------------------------------------------------------*/
/* exit thread                                                       */
/*-------------------------------------------------------------------*/
void ithread_exit(long retval)
{
	#ifdef __unix
	pthread_exit(NULL);
	#elif defined(_WIN32)
	_endthread();
	#endif
}

/*-------------------------------------------------------------------*/
/* join thread                                                       */
/*-------------------------------------------------------------------*/
int ithread_join(ilong id)
{
	long status;
	#ifdef __unix
	void *lptr = &status;
	status = pthread_join((pthread_t)id, &lptr);
	#elif defined(_WIN32)
	status = WaitForSingleObject((HANDLE)id, INFINITE);
	#endif
	return status;
}

/*-------------------------------------------------------------------*/
/* detach thread                                                     */
/*-------------------------------------------------------------------*/
int ithread_detach(ilong id)
{
	long status;
	#ifdef __unix
	status = pthread_detach((pthread_t)id);
	#elif defined(_WIN32)
	CloseHandle((HANDLE)id);
	status = 0;
	#endif
	return status;
}

/*-------------------------------------------------------------------*/
/* kill thread                                                       */
/*-------------------------------------------------------------------*/
int ithread_kill(ilong id)
{
	int retval = 0;
	#ifdef __unix
	pthread_cancel((pthread_t)id);
	retval = 0;
	#elif defined(_WIN32) 
	#ifndef _XBOX
	TerminateThread((HANDLE)id, 0);
	retval = 0;
	#else
	retval = -1;
	#endif
	#endif
	return retval;
}



/*===================================================================*/
/* Cross-Platform Socket Interface                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/* create socket                                                     */
/*-------------------------------------------------------------------*/
int isocket(int family, int type, int protocol)
{
	return (int)socket(family, type, protocol);
}

/*-------------------------------------------------------------------*/
/* close socket                                                      */
/*-------------------------------------------------------------------*/
int iclose(int sock)
{
	int retval = 0;
	if (sock < 0) return 0;
	#ifdef __unix
	retval = close(sock);
	#else
	retval = closesocket((SOCKET)sock);
	#endif
	return retval;
}

/*-------------------------------------------------------------------*/
/* connect to remote                                                 */
/*-------------------------------------------------------------------*/
int iconnect(int sock, const struct sockaddr *addr)
{
	DSOCKLEN_T len = sizeof(struct sockaddr);
	return connect(sock, addr, len);
}

/*-------------------------------------------------------------------*/
/* shutdown socket                                                   */
/*-------------------------------------------------------------------*/
int ishutdown(int sock, int mode)
{
	return shutdown(sock, mode);
}

/*-------------------------------------------------------------------*/
/* bind to local address                                             */
/*-------------------------------------------------------------------*/
int ibind(int sock, const struct sockaddr *addr)
{
	DSOCKLEN_T len = sizeof(struct sockaddr);
	return bind(sock, addr, len);
}

/*-------------------------------------------------------------------*/
/* listen                                                            */
/*-------------------------------------------------------------------*/
int ilisten(int sock, int count)
{
	return listen(sock, count);
}

/*-------------------------------------------------------------------*/
/* accept connection                                                 */
/*-------------------------------------------------------------------*/
int iaccept(int sock, struct sockaddr *addr)
{
	DSOCKLEN_T len = sizeof(struct sockaddr);
	return (int)accept(sock, addr, &len);
}

/*-------------------------------------------------------------------*/
/* get error number                                                  */
/*-------------------------------------------------------------------*/
int ierrno(void)
{
	int retval;
	#ifdef __unix
	retval = errno;
	#else
	retval = (int)WSAGetLastError();
	#endif
	return retval;
}

/*-------------------------------------------------------------------*/
/* send data                                                         */
/*-------------------------------------------------------------------*/
int isend(int sock, const void *buf, long size, int mode)
{
	return send(sock, (char*)buf, size, mode);
}

/*-------------------------------------------------------------------*/
/* receive data                                                      */
/*-------------------------------------------------------------------*/
int irecv(int sock, void *buf, long size, int mode)
{
	return recv(sock, (char*)buf, size, mode);
}

/*-------------------------------------------------------------------*/
/* send to remote                                                    */
/*-------------------------------------------------------------------*/
int isendto(int sock, const void *buf, long size, int mode, 
			const struct sockaddr *addr)
{
	DSOCKLEN_T len = sizeof(struct sockaddr);
	return sendto(sock, (char*)buf, size, mode, addr, len);
}

/*-------------------------------------------------------------------*/
/* recvfrom                                                          */
/*-------------------------------------------------------------------*/
int irecvfrom(int sock, void *buf, long size, int mode, 
			struct sockaddr *addr)
{
	DSOCKLEN_T len = sizeof(struct sockaddr);
	int retval = 0;
	retval = recvfrom(sock, (char*)buf, size, mode, addr, &len);
	return retval;
}

/*-------------------------------------------------------------------*/
/* i/o control                                                       */
/*-------------------------------------------------------------------*/
int iioctl(int sock, long cmd, unsigned long *argp)
{
	int retval;
	#ifdef __unix
	retval = ioctl(sock, cmd, argp);
	#else
	retval = ioctlsocket((SOCKET)sock, cmd, argp);
	#endif
	return retval;	
}

/*-------------------------------------------------------------------*/
/* set socket option                                                 */
/*-------------------------------------------------------------------*/
int isetsockopt(int sock, int level, int optname, const char *optval, 
	int optlen)
{
	DSOCKLEN_T len = optlen;
	return setsockopt(sock, level, optname, optval, len);
}

/*-------------------------------------------------------------------*/
/* get socket option                                                 */
/*-------------------------------------------------------------------*/
int igetsockopt(int sock, int level, int optname, char *optval, int *optlen)
{
	DSOCKLEN_T len = (optlen)? *optlen : 0;
	int retval;
	retval = getsockopt(sock, level, optname, optval, &len);
	if (optlen) *optlen = len;

	return retval;
}

/*-------------------------------------------------------------------*/
/* get socket name                                                   */
/*-------------------------------------------------------------------*/
int isockname(int sock, struct sockaddr *addr)
{
	DSOCKLEN_T len = sizeof(struct sockaddr);
	return getsockname(sock, addr, &len);
}

/*-------------------------------------------------------------------*/
/* get peer name                                                     */
/*-------------------------------------------------------------------*/
int ipeername(int sock, struct sockaddr *addr)
{
	DSOCKLEN_T len = sizeof(struct sockaddr);
	return getpeername(sock, addr, &len);
}


/*===================================================================*/
/* Basic Function Definition                                         */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/* enable option                                                     */
/*-------------------------------------------------------------------*/
int ienable(int fd, int mode)
{
	long value = 1;
	long retval = 0;

	switch (mode)
	{
	case ISOCK_NOBLOCK:
		retval = iioctl(fd, (int)FIONBIO, (unsigned long*)(void*)&value);
		break;
	case ISOCK_REUSEADDR:
		retval = isetsockopt(fd, (int)SOL_SOCKET, SO_REUSEADDR, 
			(char*)&value, sizeof(value));
		break;
	case ISOCK_NODELAY:
		retval = isetsockopt(fd, (int)IPPROTO_TCP, TCP_NODELAY, 
			(char*)&value, sizeof(value));
		break;
	case ISOCK_NOPUSH:
		#ifdef TCP_NOPUSH
		retval = isetsockopt(fd, (int)IPPROTO_TCP, TCP_NOPUSH, 
			(char*)&value, sizeof(value));
		#else
		retval = -1000;
		#endif
		break;
	}

	return retval;
}

/*-------------------------------------------------------------------*/
/* disable option                                                    */
/*-------------------------------------------------------------------*/
int idisable(int fd, int mode)
{
	long value = 0;
	long retval = 0;

	switch (mode)
	{
	case ISOCK_NOBLOCK:
		retval = iioctl(fd, (int)FIONBIO, (unsigned long*)&value);
		break;
	case ISOCK_REUSEADDR:
		retval = isetsockopt(fd, (int)SOL_SOCKET, SO_REUSEADDR, 
				(char*)&value, sizeof(value));
		break;
	case ISOCK_NODELAY:
		retval = isetsockopt(fd, (int)IPPROTO_TCP, TCP_NODELAY, 
				(char*)&value, sizeof(value));
		break;
	case ISOCK_NOPUSH:
		#ifdef TCP_NOPUSH
		retval = isetsockopt(fd, (int)IPPROTO_TCP, TCP_NOPUSH, 
				(char*)&value, sizeof(value));
		#else
		retval = -1000;
		#endif
		break;
	}
	return retval;
}

/*-------------------------------------------------------------------*/
/* poll event                                                        */
/*-------------------------------------------------------------------*/
int ipollfd(int sock, int event, long millsec)
{
	int retval = 0;

	#ifdef __unix
	struct pollfd pfd;
	
	pfd.fd = sock;
	pfd.events = 0;
	pfd.revents = 0;

	pfd.events |= (event & ISOCK_ERECV)? POLLIN : 0;
	pfd.events |= (event & ISOCK_ESEND)? POLLOUT : 0;
	pfd.events |= (event & ISOCK_ERROR)? POLLERR : 0;

	poll(&pfd, 1, millsec);

	if ((event & ISOCK_ERECV) && (pfd.revents & POLLIN)) 
		retval |= ISOCK_ERECV;
	if ((event & ISOCK_ESEND) && (pfd.revents & POLLOUT)) 
		retval |= ISOCK_ESEND;
	if ((event & ISOCK_ERROR) && (pfd.revents & POLLERR)) 
		retval |= ISOCK_ERROR;

	#else
	struct timeval tmx = { 0, 0 };
	union { void *ptr; fd_set *fds; } p[3];
	int fdr[2], fdw[2], fde[2];

	tmx.tv_sec = millsec / 1000;
	tmx.tv_usec = (millsec % 1000) * 1000;
	fdr[0] = fdw[0] = fde[0] = 1;
	fdr[1] = fdw[1] = fde[1] = sock;

	p[0].ptr = (event & ISOCK_ERECV)? fdr : NULL;
	p[1].ptr = (event & ISOCK_ESEND)? fdw : NULL;
	p[2].ptr = (event & ISOCK_ESEND)? fde : NULL;

	retval = select( sock + 1, p[0].fds, p[1].fds, p[2].fds, 
					(millsec >= 0)? &tmx : 0);
	retval = 0;

	if ((event & ISOCK_ERECV) && fdr[0]) retval |= ISOCK_ERECV;
	if ((event & ISOCK_ESEND) && fdw[0]) retval |= ISOCK_ESEND;
	if ((event & ISOCK_ERROR) && fde[0]) retval |= ISOCK_ERROR;
	#endif

	return retval;
}

/*-------------------------------------------------------------------*/
/* send all data                                                     */
/*-------------------------------------------------------------------*/
int isendall(int sock, const void *buf, long size)
{
	unsigned char *lptr = (unsigned char*)buf;
	int total = 0, retval = 0, c;

	for (; size > 0; lptr += retval, size -= (long)retval) {
		retval = isend(sock, lptr, size, 0);
		if (retval == 0) {
			retval = -1;
			break;
		}
		if (retval == -1) {
			c = ierrno();
			if (c != IEAGAIN) {
				retval = -1000 - c;
				break;
			}
			retval = 0;
			break;
		}
		total += retval;
	}

	return (retval < 0)? retval : total;
}

/*-------------------------------------------------------------------*/
/* try to receive all data                                           */
/*-------------------------------------------------------------------*/
int irecvall(int sock, void *buf, long size)
{
	unsigned char *lptr = (unsigned char*)buf;
	int total = 0, retval = 0, c;

	for (; size > 0; lptr += retval, size -= (long)retval) {
		retval = irecv(sock, lptr, size, 0);
		if (retval == 0) {
			retval = -1;
			break;
		}
		if (retval == -1) {
			c = ierrno();
			if (c != IEAGAIN) {
				retval = -1000 - c;
				break;
			}
			retval = 0;
			break;
		}
		total += retval;
	}

	return (retval < 0)? retval : total;
}

/*-------------------------------------------------------------------*/
/* format error string                                               */
/*-------------------------------------------------------------------*/
char *ierrstr(int errnum, char *msg, int size)
{
	static char buffer[1025];
	char *lptr = (msg == NULL)? buffer : msg;
	long length = (msg == NULL)? 1024 : size;
	#ifdef __unix
	strncpy(lptr, strerror(errnum), length);
	#elif (!defined(_XBOX))
	LPVOID lpMessageBuf;
	fd_set fds;
	FD_ZERO(&fds);
	FD_CLR(0, &fds);
	size = (long)FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, errnum, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), 
		(LPTSTR) &lpMessageBuf, 0, NULL);
	strncpy(lptr, (char*)lpMessageBuf, length);
	LocalFree(lpMessageBuf);
	#else
	sprintf(buffer, "XBOX System Error Code: %d", errnum);
	strncpy(msg, buffer, length);
	#endif
	return lptr;
}

/*-------------------------------------------------------------------*/
/* get host address                                                  */
/*-------------------------------------------------------------------*/
int igethostaddr(struct in_addr *addrs, int count)
{
	#ifndef _XBOX
	struct hostent *h;
	char szhn[256];
	char **hp;
	int i;

	if (gethostname(szhn, 256)) {
		return -1;
	}
	h = gethostbyname(szhn);
	if (h == NULL) return -2;
	if (h->h_addr_list == NULL) return -3;
	hp = h->h_addr_list;
	for (i = 0 ; i < count && hp[i]; i++) {
		addrs[i] = *(struct in_addr*)hp[i];
	}
	count = i;
	#else
	XNADDR pxna;
	XNetGetTitleXnAddr(&pxna);
	if (count >= 1) {
		addrs[0] = pxna.ina;
		count = 1;
	}
	#endif
	return count;
}

/*-------------------------------------------------------------------*/
/* sockaddr operations                                               */
/*-------------------------------------------------------------------*/

/* set address of sockaddr */
void isockaddr_set_ip(struct sockaddr *a, unsigned long ip)
{
	struct sockaddr_in *addr = (struct sockaddr_in*)a;
	addr->sin_addr.s_addr = htonl(ip);
}

/* get address of sockaddr */
unsigned long isockaddr_get_ip(const struct sockaddr *a)
{
	struct sockaddr_in *addr = (struct sockaddr_in*)a;
	return ntohl(addr->sin_addr.s_addr);
}

/* set port of sockaddr */
void isockaddr_set_port(struct sockaddr *a, int port)
{
	struct sockaddr_in *addr = (struct sockaddr_in*)a;
	addr->sin_port = htons((short)port);
}

/* get port of sockaddr */
int isockaddr_get_port(struct sockaddr *a)
{
	struct sockaddr_in *addr = (struct sockaddr_in*)a;
	return ntohs(addr->sin_port);
}

/* set family */
void isockaddr_set_family(struct sockaddr *a, int family)
{
	struct sockaddr_in *addr = (struct sockaddr_in*)a;
	addr->sin_family = family;
}

/* get family */
int isockaddr_get_family(struct sockaddr *a)
{
	struct sockaddr_in *addr = (struct sockaddr_in*)a;
	return addr->sin_family;
}

/* setup sockaddr */
struct sockaddr* isockaddr_set(struct sockaddr *a, unsigned long ip, int p)
{
	struct sockaddr_in *addr = (struct sockaddr_in*)a;
	memset(a, 0, sizeof(struct sockaddr));
	addr->sin_family = AF_INET;
	isockaddr_set_ip(a, ip);
	isockaddr_set_port(a, p);
	return a;
}

/* set text to ip */
int isockaddr_set_ip_text(struct sockaddr *a, const char *text)
{
	struct sockaddr_in *addr = (struct sockaddr_in*)a;
	int is_name = 0, i;

	for (i = 0; text[i]; i++) {
		if (!((text[i] >= '0' && text[i] <= '9') || text[i] == '.')) {
			is_name = 1;
			break;
		}
	}

	if (is_name) {
		#if defined(__MACH__)
		struct hostent * he = gethostbyname(text);
		if (he == NULL) return -1;
		if (he->h_length != 4) return -2;
		memcpy((char*)&(addr->sin_addr), he->h_addr_list[0], he->h_length);
		#elif !defined(_XBOX)
		struct hostent * he = gethostbyname(text);
		if (he == NULL) return -1;
		if (he->h_length != 4) return -2;
		memcpy((char*)&(addr->sin_addr), he->h_addr, he->h_length);
		#else
		WSAEVENT hEvent = WSACreateEvent();
		XNDNS * pxndns = NULL;
		INT err = XNetDnsLookup(text, hEvent, &pxndns);
		WaitForSingleObject(hEvent, INFINITE);
		if (pxndns->iStatus != 0) {
			XNetDnsRelease(pxndns);
			return -1;
		}
		memcpy((char*)&(addr->sin_addr), &pxndns->aina[0].S_un.S_addr, 
			sizeof(addr->sin_addr));
		XNetDnsRelease(pxndns);
		#endif
		return 0;
	}
	
	addr->sin_addr.s_addr = inet_addr(text);

	return 0;
}

/* make up address */
struct sockaddr *isockaddr_makeup(struct sockaddr *a, const char *ip, int p)
{
	static struct sockaddr static_addr;
	if (a == NULL) a = &static_addr;
	memset(a, 0, sizeof(struct sockaddr));
	isockaddr_set_family(a, AF_INET);
	isockaddr_set_ip_text(a, ip);
	isockaddr_set_port(a, p);
	return a;
}

/* convert address to text */
char *isockaddr_str(const struct sockaddr *a, char *text)
{
	struct sockaddr_in *addr = (struct sockaddr_in*)a;
	static char buffer[1025];
	unsigned char *bytes;
	int ipb[5], i;
	bytes = (unsigned char*)&(addr->sin_addr.s_addr);
	for (i = 0; i < 4; i++) ipb[i] = bytes[i];
	ipb[4] = (int)(ntohs(addr->sin_port));
	text = text? text : buffer;
	sprintf(text, "%d.%d.%d.%d:%d", ipb[0], ipb[1], ipb[2], ipb[3], ipb[4]);
	return text;
}

/* compare two addresses */
int isockaddr_cmp(const struct sockaddr *a, const struct sockaddr *b)
{
	struct sockaddr_in *x = (struct sockaddr_in*)a;
	struct sockaddr_in *y = (struct sockaddr_in*)b;
	unsigned long addr1 = ntohl(x->sin_addr.s_addr);
	unsigned long addr2 = ntohl(y->sin_addr.s_addr);
	int port1 = ntohs(x->sin_port);
	int port2 = ntohs(y->sin_port);
	if (addr1 > addr2) return 10;
	if (addr1 < addr2) return -10;
	if (port1 > port2) return 1;
	if (port1 < port2) return -1;
	return 0;
}


/*===================================================================*/
/* Simple Assistant Function                                         */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/* init network                                                      */
/*-------------------------------------------------------------------*/
int inet_init(void)
{
	#if defined(_WIN32) || defined(WIN32)
	static int inited = 0;
	WSADATA WSAData;
	int retval = 0;

	#ifdef _XBOX
    XNetStartupParams xnsp;
	#endif

	if (inited == 0) {
		#ifdef _XBOX
		memset(&xnsp, 0, sizeof(xnsp));
		xnsp.cfgSizeOfStruct = sizeof(XNetStartupParams);
		xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
		XNetStartup(&xnsp);
		#endif

		retval = WSAStartup(0x202, &WSAData);
		if (WSAData.wVersion != 0x202) {
			WSACleanup();
			fprintf(stderr, "WSAStartup failed !!\n");
			fflush(stderr);
			return -1;
		}

		inited = 1;
	}

	#elif defined(__unix)
	static int inited = 0;
	if (inited == 0) {
		signal(SIGPIPE, SIG_IGN);
		inited = 1;
	}
	#endif

	return 0;
}


/*-------------------------------------------------------------------*/
/* open udp port                                                     */
/*-------------------------------------------------------------------*/
int inet_open_port(unsigned short port, unsigned long ip, int noblock)
{
	struct sockaddr addr;
	static int inited = 0;
	long mode = 1;
	int sock;

	if (inited == 0) {
		inet_init();
		inited = 1;
	}

	sock = (int)socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) return -1;

	memset(&addr, 0, sizeof(addr));
	isockaddr_set(&addr, ip, port);

	if (bind(sock, &addr, sizeof(addr)) != 0) {
		iclose(sock);
		return -2;
	}

#if (defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(WIN64))
	#define _SIO_UDP_CONNRESET_ _WSAIOW(IOC_VENDOR, 12)
	{
		DWORD dwBytesReturned = 0;
		BOOL bNewBehavior = FALSE;
		DWORD status;
		// disable  new behavior using
		// IOCTL: SIO_UDP_CONNRESET

		status = WSAIoctl(sock, _SIO_UDP_CONNRESET_,
					&bNewBehavior, sizeof(bNewBehavior),  
					NULL, 0, &dwBytesReturned, 
					NULL, NULL);

		if (SOCKET_ERROR == (int)status) {
			DWORD err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK) {
				// nothing to doreturn(FALSE);
			} else {
				printf("WSAIoctl(SIO_UDP_CONNRESET) Error: %d\n", (int)err);
				iclose(sock);
				return -3;
			}
		}
	}
#endif

	if (noblock != 0) {
		ienable(sock, ISOCK_NOBLOCK);
	}

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&mode, sizeof(long));

	return sock;
}

/*-------------------------------------------------------------------*/
/* set recv buf and send buf                                         */
/*-------------------------------------------------------------------*/
int inet_set_bufsize(int sock, long rcvbuf_size, long sndbuf_size)
{
	long len = sizeof(long);
	int retval;
	if (rcvbuf_size > 0) {
		retval = isetsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&rcvbuf_size, len);
		if (retval < 0) return retval;
	}
	if (sndbuf_size > 0) {
		retval = isetsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&sndbuf_size, len);
		if (retval < 0) return retval;
	}
	return 0;
}



/*===================================================================*/
/* Cross-Platform Poll Interface                                     */
/*===================================================================*/
#if defined(_WIN32) || defined(__unix)
#define IHAVE_SELECT
#endif
#if defined(__unix)
#define IHAVE_POLL
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define IHAVE_KEVENT
#endif
#if defined(linux)
#define IHAVE_EPOLL
//#define IHAVE_RTSIG
#endif
#if defined(_WIN32)
//#define IHAVE_WINCP
#endif
#if defined(sun)
//#define IHAVE_DEVPOLL
#endif

#if defined(__MACH__) && (!defined(IHAVE_KEVENT))
#define IHAVE_KEVENT
#endif


struct IPOLL_DRIVER
{
	int pdsize;								/* poll descriptor size */
	int id;									/* device id */
	int performance;						/* performance value */
	const char *name;						/* device name */
	int (*startup)(void);					/* init device */
	int (*shutdown)(void);					/* shutdown device */
	int (*init_pd)(ipolld ipd, int param);	/* create poll-fd */
	int (*destroy_pd)(ipolld ipd);			/* delete poll-fd */
	int (*poll_add)(ipolld ipd, int fd, int mask, void *udata);	
	int (*poll_del)(ipolld ipd, int fd);						
	int (*poll_set)(ipolld ipd, int fd, int mask);		
	int (*poll_wait)(ipolld ipd, int timeval);			
	int (*poll_event)(ipolld ipd, int *fd, int *event, void **udata);
};

/* current poll device */
struct IPOLL_DRIVER IPOLLDRV;	

#define PSTRUCT void					// 定义基本结构体
#define PDESC(pd) ((PSTRUCT*)(pd))		// 定义结构体转换

/* poll file descriptor */
struct IPOLLFD	
{
	int fd;		/* file descriptor */
	int mask;	/* event mask */
	int event;	/* event */
	int index;	/* table index */
	void*user;	/* user data */
};

/* poll resize-able vector */
struct IPVECTOR
{
	unsigned char *data;
	long length;
	long block;
};

/* file descriptor vector */
struct IPOLLFV
{
	struct IPOLLFD *fds;
	struct IPVECTOR vec;
	long count;
};


/*-------------------------------------------------------------------------*/
/* Support Poll Device                                                     */
/*-------------------------------------------------------------------------*/
#ifdef IHAVE_SELECT
extern struct IPOLL_DRIVER IPOLL_SELECT;
#endif
#ifdef IHAVE_POLL
extern struct IPOLL_DRIVER IPOLL_POLL;
#endif
#ifdef IHAVE_KEVENT
extern struct IPOLL_DRIVER IPOLL_KEVENT;
#endif
#ifdef IHAVE_WINCP
extern struct IPOLL_DRIVER IPOLL_WINCP;
#endif
#ifdef IHAVE_EPOLL
extern struct IPOLL_DRIVER IPOLL_EPOLL;
#endif
#ifdef IHAVE_DEVPOLL
extern struct IPOLL_DRIVER IPOLL_DEVPOLL;
#endif
#ifdef IHAVE_RTSIG
extern struct IPOLL_DRIVER IPOLL_RTSIG;
#endif

static struct IPOLL_DRIVER *ipoll_list[] = {
#ifdef IHAVE_SELECT
	&IPOLL_SELECT,
#endif
#ifdef IHAVE_POLL
	&IPOLL_POLL,
#endif
#ifdef IHAVE_KEVENT
	&IPOLL_KEVENT,
#endif
#ifdef IHAVE_EPOLL
	&IPOLL_EPOLL,
#endif
#ifdef IHAVE_RTSIG
	&IPOLL_RTSIG,
#endif
#ifdef IHAVE_WINCP
	&IPOLL_WINCP,
#endif
#ifdef IHAVE_DEVPOLL
	&IPOLL_DEVPOLL,
#endif
	NULL
};


static int ipoll_inited = 0;
IMUTEX_TYPE ipoll_mutex;


/*-------------------------------------------------------------------*/
/* poll device init                                                  */
/*-------------------------------------------------------------------*/
int ipoll_init(int device)
{
	int besti, bestv;
	int retval, i;

	if (ipoll_inited) return 1;
	
	if (device != IDEVICE_AUTO && device >= 0) {
		for (i = 0; ipoll_list[i]; i++) 
			if (ipoll_list[i]->id == device) break;
		if (ipoll_list[i] == NULL) 
			return -1;
		IPOLLDRV = *ipoll_list[i];
	}	else {
		besti = 0;
		bestv = -1;
		for (i = 0; ipoll_list[i]; i++) {
			if (ipoll_list[i]->performance > bestv) {
				bestv = ipoll_list[i]->performance;
				besti = i;
			}
		}
		IPOLLDRV = *ipoll_list[besti];
	}
	
	retval = IPOLLDRV.startup();

	if (retval != 0) return -2;

	IMUTEX_INIT(&ipoll_mutex);
	ipoll_inited = 1;

	return 0;
}

/*-------------------------------------------------------------------*/
/* poll device quit                                                  */
/*-------------------------------------------------------------------*/
int ipoll_quit(void)
{
	if (ipoll_inited == 0) return 0;
	IPOLLDRV.shutdown();
	IMUTEX_DESTROY(&ipoll_mutex);
	ipoll_inited = 0;
	return 0;
}

/*-------------------------------------------------------------------*/
/* pfd create                                                        */
/*-------------------------------------------------------------------*/
int ipoll_create(ipolld *ipd, int param)
{
	ipolld pd;

	assert(ipd && ipoll_inited);
	if (ipd == NULL || ipoll_inited == 0) return -1;

	pd = (ipolld)malloc(IPOLLDRV.pdsize);
	if (pd == NULL) return -2;

	if (IPOLLDRV.init_pd(pd, param)) {
		free(pd);
		ipd[0] = NULL;
		return -3;
	}

	ipd[0] = pd;
	return 0;
}

/*-------------------------------------------------------------------*/
/* pfd delete                                                        */
/*-------------------------------------------------------------------*/
int ipoll_delete(ipolld ipd)
{
	int retval;
	assert(ipd && ipoll_inited);
	if (ipd == NULL || ipoll_inited == 0) return -1;
	retval = IPOLLDRV.destroy_pd(ipd);
	free(ipd);
	return retval;
}

/*-------------------------------------------------------------------*/
/* add file descriptor into pfd                                      */
/*-------------------------------------------------------------------*/
int ipoll_add(ipolld ipd, int fd, int mask, void *udata)
{
	return IPOLLDRV.poll_add(ipd, fd, mask, udata);
}

/*-------------------------------------------------------------------*/
/* delete file descriptor from pfd                                   */
/*-------------------------------------------------------------------*/
int ipoll_del(ipolld ipd, int fd)
{
	return IPOLLDRV.poll_del(ipd, fd);
}

/*-------------------------------------------------------------------*/
/* set file event                                                    */
/*-------------------------------------------------------------------*/
int ipoll_set(ipolld ipd, int fd, int mask)
{
	return IPOLLDRV.poll_set(ipd, fd, mask);
}

/*-------------------------------------------------------------------*/
/* wait for event                                                    */
/*-------------------------------------------------------------------*/
int ipoll_wait(ipolld ipd, int millisecond)
{
	return IPOLLDRV.poll_wait(ipd, millisecond);
}

/*-------------------------------------------------------------------*/
/* get each event                                                    */
/*-------------------------------------------------------------------*/
int ipoll_event(ipolld ipd, int *fd, int *event, void **udata)
{
	int retval;
	do {
		retval = IPOLLDRV.poll_event(ipd, fd, event, udata);
	}	while (*event == 0 && retval == 0);
	return retval;
}

void * (*inet_malloc)(size_t) = NULL;
void (*inet_free)(void *ptr) = NULL;


/*-------------------------------------------------------------------*/
/* vector init                                                       */
/*-------------------------------------------------------------------*/
static void ipv_init(struct IPVECTOR *vec)
{
	vec->data = NULL;
	vec->length = 0;
	vec->block = 0;
}


/*-------------------------------------------------------------------*/
/* vector destroy                                                    */
/*-------------------------------------------------------------------*/
static void ipv_destroy(struct IPVECTOR *vec)
{
	assert(vec);
	if (vec->data) {
		if (inet_free) inet_free(vec->data);
		else free(vec->data);
	}
	vec->data = NULL;
	vec->length = 0;
	vec->block = 0;
}

/*-------------------------------------------------------------------*/
/* vector resize                                                     */
/*-------------------------------------------------------------------*/
static int ipv_resize(struct IPVECTOR *v, long newsize)
{
	unsigned char*lptr;
	unsigned long block, min;
	unsigned long nblock;

	if (v == NULL) return -1;
	if (newsize > v->length && newsize <= v->block) { 
		v->length = newsize; 
		return 0; 
	}

	for (nblock = 1; nblock < (unsigned long)newsize; ) nblock <<= 1;
	block = nblock;

	if (block == (unsigned long)v->block) { 
		v->length = newsize; 
		return 0; 
	}

	if (v->block == 0 || v->data == NULL) {
		if (inet_malloc) v->data = (unsigned char*)inet_malloc(block);
		else v->data = (unsigned char*)malloc(block);
		if (v->data == NULL) return -1;
		v->length = newsize;
		v->block = block;
	}    else {
		if (inet_malloc) lptr = (unsigned char*)inet_malloc(block);
		else lptr = (unsigned char*)malloc(block);
		if (lptr == NULL) return -1;

		min = (v->length <= newsize)? v->length : newsize;
		memcpy(lptr, v->data, (size_t)min);
		if (inet_free) inet_free(v->data);
		else free(v->data);

		v->data = lptr;
		v->length = newsize;
		v->block = block;
	}
	return 0;
}

/*-------------------------------------------------------------------*/
/* ipoll fv init                                                     */
/*-------------------------------------------------------------------*/
static void ipoll_fvinit(struct IPOLLFV *fv)
{
	fv->fds = NULL;
	fv->count = 0;
	ipv_init(&fv->vec);
}

/*-------------------------------------------------------------------*/
/* ipoll fv init                                                     */
/*-------------------------------------------------------------------*/
static void ipoll_fvdestroy(struct IPOLLFV *fv)
{
	assert(fv);
	ipv_destroy(&fv->vec);
	fv->fds = NULL;
	fv->count = 0;
}

/*-------------------------------------------------------------------*/
/* ipoll fv init                                                     */
/*-------------------------------------------------------------------*/
static int ipoll_fvresize(struct IPOLLFV *fv, long count)
{
	int retval;
	retval = ipv_resize(&fv->vec, count * sizeof(struct IPOLLFD));
	assert(retval == 0);
	fv->fds = (struct IPOLLFD*)fv->vec.data;
	fv->count = count;
	return 0;
}


/*===================================================================*/
/* POLL DRIVER - SELECT                                              */
/*===================================================================*/
#ifdef IHAVE_SELECT

static int ips_startup(void);
static int ips_shutdown(void);
static int ips_init_pd(ipolld ipd, int param);
static int ips_destroy_pd(ipolld ipd);
static int ips_poll_add(ipolld ipd, int fd, int mask, void *user);
static int ips_poll_del(ipolld ipd, int fd);
static int ips_poll_set(ipolld ipd, int fd, int mask);
static int ips_poll_wait(ipolld ipd, int timeval);
static int ips_poll_event(ipolld ipd, int *fd, int *event, void **user);


/*-------------------------------------------------------------------*/
/* POLL DESCRIPTOR - SELECT                                          */
/*-------------------------------------------------------------------*/
typedef struct
{
	struct IPOLLFV fv;
	fd_set fdr, fdw, fde;
	fd_set fdrtest, fdwtest, fdetest;
	int max_fd;
	int min_fd;
	int cur_fd;
	int cnt_fd;
	int rbits;
}	IPD_SELECT;

/*-------------------------------------------------------------------*/
/* POLL DRIVER - SELECT                                              */
/*-------------------------------------------------------------------*/
struct IPOLL_DRIVER IPOLL_SELECT = {
	sizeof (IPD_SELECT),
	IDEVICE_SELECT,
	0,
	"SELECT",
	ips_startup,
	ips_shutdown,
	ips_init_pd,
	ips_destroy_pd,
	ips_poll_add,
	ips_poll_del,
	ips_poll_set,
	ips_poll_wait,
	ips_poll_event
};

#ifdef PSTRUCT
#undef PSTRUCT
#endif

#define PSTRUCT IPD_SELECT


/*-------------------------------------------------------------------*/
/* startup select device                                             */
/*-------------------------------------------------------------------*/
static int ips_startup(void)
{
	return 0;
}

/*-------------------------------------------------------------------*/
/* shutdown select device                                            */
/*-------------------------------------------------------------------*/
static int ips_shutdown(void)
{
	return 0;
}

/*-------------------------------------------------------------------*/
/* init select descriptor                                            */
/*-------------------------------------------------------------------*/
static int ips_init_pd(ipolld ipd, int param)
{
	int retval = 0;
	PSTRUCT *ps = PDESC(ipd);
	ps->max_fd = 0;
	ps->min_fd = 0x7fffffff;
	ps->cur_fd = 0;
	ps->cnt_fd = 0;
	ps->rbits = 0;
	FD_ZERO(&ps->fdr);
	FD_ZERO(&ps->fdw);
	FD_ZERO(&ps->fde);
	param = param;
	ipoll_fvinit(&ps->fv);
	
	if (ipoll_fvresize(&ps->fv, 4)) {
		retval = ips_destroy_pd(ipd);
		return -2;
	}

	return retval;
}

/*-------------------------------------------------------------------*/
/* destroy select descriptor                                         */
/*-------------------------------------------------------------------*/
static int ips_destroy_pd(ipolld ipd)
{
	PSTRUCT *ps = PDESC(ipd);
	ipoll_fvdestroy(&ps->fv);
	return 0;
}

/*-------------------------------------------------------------------*/
/* add file descriptor                                               */
/*-------------------------------------------------------------------*/
static int ips_poll_add(ipolld ipd, int fd, int mask, void *user)
{
	PSTRUCT *ps = PDESC(ipd);
	int oldmax = ps->max_fd, i;

	#ifdef __unix
	if (fd >= FD_SETSIZE) return -1;
	#else
	if (ps->cnt_fd >= FD_SETSIZE) return -1;
	#endif

	if (ps->max_fd < fd) ps->max_fd = fd;
	if (ps->min_fd > fd) ps->min_fd = fd;
	if (mask & IPOLL_IN) FD_SET((unsigned)fd, &ps->fdr);
	if (mask & IPOLL_OUT) FD_SET((unsigned)fd, &ps->fdw);
	if (mask & IPOLL_ERR) FD_SET((unsigned)fd, &ps->fde);

	if (ipoll_fvresize(&ps->fv, ps->max_fd + 2)) return -2;

	for (i = oldmax + 1; i <= ps->max_fd; i++) {
		ps->fv.fds[i].fd = -1;
	}
	ps->fv.fds[fd].fd = fd;
	ps->fv.fds[fd].user = user;
	ps->fv.fds[fd].mask = mask;

	ps->cnt_fd++;

	return 0;
}

/*-------------------------------------------------------------------*/
/* delete file descriptor                                            */
/*-------------------------------------------------------------------*/
static int ips_poll_del(ipolld ipd, int fd)
{
	PSTRUCT *ps = PDESC(ipd);
	int mask = 0;

	if (fd > ps->max_fd) return -1;
	mask = ps->fv.fds[fd].mask;
	if (ps->fv.fds[fd].fd < 0) return -2;

	if (mask & IPOLL_IN) FD_CLR((unsigned)fd, &ps->fdr);
	if (mask & IPOLL_OUT) FD_CLR((unsigned)fd, &ps->fdw);
	if (mask & IPOLL_ERR) FD_CLR((unsigned)fd, &ps->fde);

	ps->fv.fds[fd].fd = -1;
	ps->fv.fds[fd].user = NULL;
	ps->fv.fds[fd].mask = 0;

	ps->cnt_fd--;

	return 0;
}

/*-------------------------------------------------------------------*/
/* set event mask                                                    */
/*-------------------------------------------------------------------*/
static int ips_poll_set(ipolld ipd, int fd, int mask)
{
	PSTRUCT *ps = PDESC(ipd);
	int omask = 0;

	if (ps->fv.fds[fd].fd < 0) return -1;
	omask = ps->fv.fds[fd].mask;

	if (omask & IPOLL_IN) {
		if (!(mask & IPOLL_IN)) FD_CLR((unsigned)fd, &ps->fdr);
	}	else {
		if (mask & IPOLL_IN) FD_SET((unsigned)fd, &ps->fdr);
	}
	if (omask & IPOLL_OUT) {
		if (!(mask & IPOLL_OUT)) FD_CLR((unsigned)fd, &ps->fdw);
	}	else {
		if (mask & IPOLL_OUT) FD_SET((unsigned)fd, &ps->fdw);
	}
	if (omask & IPOLL_IN) {
		if (!(mask & IPOLL_IN)) FD_CLR((unsigned)fd, &ps->fde);
	}	else {
		if (mask & IPOLL_IN) FD_SET((unsigned)fd, &ps->fde);
	}
	ps->fv.fds[fd].mask = mask;

	return 0;
}

/*-------------------------------------------------------------------*/
/* wait event                                                        */
/*-------------------------------------------------------------------*/
static int ips_poll_wait(ipolld ipd, int timeval)
{
	PSTRUCT *ps = PDESC(ipd);
	int nbits;
	struct timeval timeout;

	timeout.tv_sec  = timeval / 1000;
	timeout.tv_usec = (timeval % 1000) * 1000;
	ps->fdrtest = ps->fdr;
	ps->fdwtest = ps->fdw;
	ps->fdetest = ps->fde;
	nbits = select(ps->max_fd + 1, &ps->fdrtest, &ps->fdwtest, 
		&ps->fdetest, &timeout);
	if (nbits < 0) return -1;

	ps->cur_fd = ps->min_fd - 1;
	ps->rbits = nbits;
	return (nbits == 0)? 0 : nbits;
}

/*-------------------------------------------------------------------*/
/* query result                                                      */
/*-------------------------------------------------------------------*/
static int ips_poll_event(ipolld ipd, int *fd, int *event, void **user)
{
	PSTRUCT *ps = PDESC(ipd);
	int revents, n;

	if (ps->rbits < 1) return -1;
	for (revents=0; ps->cur_fd++ < ps->max_fd; ) {
		if (FD_ISSET(ps->cur_fd, &ps->fdrtest)) revents = IPOLL_IN;
		if (FD_ISSET(ps->cur_fd, &ps->fdwtest)) revents |= IPOLL_OUT;
		if (FD_ISSET(ps->cur_fd, &ps->fdetest)) revents |= IPOLL_ERR;
		if (revents) break;
	}

	if (!revents) return -2;

	if (revents & IPOLL_IN)  ps->rbits--;
	if (revents & IPOLL_OUT) ps->rbits--;
	if (revents & IPOLL_ERR) ps->rbits--;

	n = ps->cur_fd;
	if (ps->fv.fds[n].fd < 0) revents = 0;
	revents &= ps->fv.fds[n].mask;

	if (fd) *fd = n;
	if (event) *event = revents;
	if (user) *user = ps->fv.fds[n].user;

	return 0;
}

#endif


/*===================================================================*/
/* POLL DRIVER - POLL                                                */
/*===================================================================*/

#ifdef IHAVE_POLL

#include <stdio.h>
#include <poll.h>

static int ipp_startup(void);
static int ipp_shutdown(void);
static int ipp_init_pd(ipolld ipd, int param);
static int ipp_destroy_pd(ipolld ipd);
static int ipp_poll_add(ipolld ipd, int fd, int mask, void *user);
static int ipp_poll_del(ipolld ipd, int fd);
static int ipp_poll_set(ipolld ipd, int fd, int mask);
static int ipp_poll_wait(ipolld ipd, int timeval);
static int ipp_poll_event(ipolld ipd, int *fd, int *event, void **user);


/*-------------------------------------------------------------------*/
/* poll desc                                                         */
/*-------------------------------------------------------------------*/
typedef struct 
{
	struct IPOLLFV fv;
	struct IPVECTOR vpollfd;
	struct IPVECTOR vresultq;
	struct pollfd *pfds;
	struct pollfd *resultq;
	long fd_max;
	long fd_min;
	long pnum_max;
	long pnum_cnt;
	long result_num;
	long result_cur;
}	IPD_POLL;

/*-------------------------------------------------------------------*/
/* poll driver                                                       */
/*-------------------------------------------------------------------*/
struct IPOLL_DRIVER IPOLL_POLL = {
	sizeof (IPD_POLL),
	IDEVICE_POLL,
	4,
	"POLL",
	ipp_startup,
	ipp_shutdown,
	ipp_init_pd,
	ipp_destroy_pd,
	ipp_poll_add,
	ipp_poll_del,
	ipp_poll_set,
	ipp_poll_wait,
	ipp_poll_event
};

#ifdef PSTRUCT
#undef PSTRUCT
#endif

#define PSTRUCT IPD_POLL

/* poll startup device */
static int ipp_startup(void)
{
	return 0;
}

/* poll shutdown */
static int ipp_shutdown(void)
{
	return 0;
}

/* poll init descriptor */
static int ipp_init_pd(ipolld ipd, int param)
{
	PSTRUCT *ps = PDESC(ipd);

	ipoll_fvinit(&ps->fv);

	ipv_init(&ps->vpollfd);
	ipv_init(&ps->vresultq);
	ps->fd_max = 0;
	ps->fd_min = 0x7fffffff;
	ps->pnum_max = 0;
	ps->pnum_cnt = 0;
	ps->result_num = -1;
	ps->result_cur = -1;
	param = param;

	return 0;
}

/* poll destroy descriptor */
static int ipp_destroy_pd(ipolld ipd)
{
	PSTRUCT *ps = PDESC(ipd);
	
	ipoll_fvdestroy(&ps->fv);
	ipv_destroy(&ps->vpollfd);
	ipv_destroy(&ps->vresultq);
	ps->fd_max = 0;
	ps->fd_min = 0x7fffffff;
	ps->pnum_max = 0;
	ps->pnum_cnt = 0;
	ps->result_num = -1;
	ps->result_cur = -1;

	return 0;
}

/* poll add file into descriptor */
static int ipp_poll_add(ipolld ipd, int fd, int mask, void *user)
{
	PSTRUCT *ps = PDESC(ipd);
	long ofd_max = (long)ps->fd_max;
	struct pollfd *pfd;
	int retval, index, i;

	if (fd > ps->fd_max) ps->fd_max = fd;
	if (fd < ps->fd_min) ps->fd_min = fd;

	if (ipoll_fvresize(&ps->fv, ps->fd_max + 2)) return -1;

	for (i = ofd_max + 1; i <= ps->fd_max; i++) {
		ps->fv.fds[i].fd = -1;
		ps->fv.fds[i].user = NULL;
		ps->fv.fds[i].mask = 0;
	}

	/* already added in */
	if (ps->fv.fds[fd].fd >= 0) return 1;

	if (ps->pnum_cnt >= ps->pnum_max) {
		i = (ps->pnum_max <= 0)? 4 : ps->pnum_max * 2;
		retval = ipv_resize(&ps->vpollfd, sizeof(struct pollfd) * i);
		if (retval) return -3;
		retval = ipv_resize(&ps->vresultq, sizeof(struct pollfd) * i);
		if (retval) return -4;
		ps->pnum_max = i;
		ps->pfds = (struct pollfd*)ps->vpollfd.data;
		ps->resultq = (struct pollfd*)ps->vresultq.data;
	}

	index = ps->pnum_cnt++;
	pfd = &ps->pfds[index];
	pfd->fd = fd;
	pfd->events = 0;
	if (mask & IPOLL_IN) pfd->events |= POLLIN;
	if (mask & IPOLL_OUT)pfd->events |= POLLOUT;
	if (mask & IPOLL_ERR)pfd->events |= POLLERR;
	pfd->revents = 0;

	ps->fv.fds[fd].fd = fd;
	ps->fv.fds[fd].index = index;
	ps->fv.fds[fd].user = user;
	ps->fv.fds[fd].mask = mask;

	return 0;
}

/* poll delete file from descriptor */
static int ipp_poll_del(ipolld ipd, int fd)
{
	PSTRUCT *ps = PDESC(ipd);
	int index, last, lastfd;

	if (fd < ps->fd_min || fd > ps->fd_max) return -1;
	if (ps->fv.fds[fd].fd < 0) return 0;
	if (ps->fv.fds[fd].index < 0) return 0;
	if (ps->pnum_cnt <= 0) return -2;

	last = ps->pnum_cnt - 1;
	index = ps->fv.fds[fd].index;
	ps->pfds[index] = ps->pfds[last];
	lastfd = ps->pfds[index].fd;
	ps->fv.fds[lastfd].index = index;
	ps->fv.fds[fd].index = -1;

	ps->fv.fds[fd].fd = -1;
	ps->fv.fds[fd].mask = 0;
	ps->fv.fds[fd].user = NULL;

	ps->pnum_cnt--;

	
	return 0;
}

/* poll set event mask */
static int ipp_poll_set(ipolld ipd, int fd, int mask)
{
	PSTRUCT *ps = PDESC(ipd);
	int index, events = 0;
	if (fd < ps->fd_min || fd > ps->fd_max) return -1;
	if (ps->fv.fds[fd].fd < 0) return 0;

	index = ps->fv.fds[fd].index;
	if (ps->pfds[index].fd != fd) return -3;
	if (mask & IPOLL_IN) events |= POLLIN;
	if (mask & IPOLL_OUT) events |= POLLOUT;
	if (mask & IPOLL_ERR) events |= POLLERR;
	ps->pfds[index].events = events;
	ps->fv.fds[fd].mask = mask;

	return 0;
}

/* poll wait events */
static int ipp_poll_wait(ipolld ipd, int timeval)
{
	PSTRUCT *ps = PDESC(ipd);
	long retval, i, j;

	retval = poll(ps->pfds, ps->pnum_cnt, timeval);
	ps->result_num = -1;
	if (retval < 0) {
		return retval;
	}
	ps->result_num = 0;
	ps->result_cur = 0;
	for (i = 0; i < ps->pnum_cnt; i++) {
		if (ps->pfds[i].revents) {
			j = ps->result_num++;
			ps->resultq[j] = ps->pfds[i];
		}
	}
	return retval;
}

/* poll query event */
static int ipp_poll_event(ipolld ipd, int *fd, int *event, void **user)
{
	PSTRUCT *ps = PDESC(ipd);
	int revents, eventx = 0, n;
	struct pollfd *pfd;
	if (ps->result_num < 0) return -1;
	if (ps->result_cur >= ps->result_num) return -2;
	pfd = &ps->resultq[ps->result_cur++];

	revents = pfd->revents;
	if (revents & POLLIN) eventx |= IPOLL_IN;
	if (revents & POLLOUT)eventx |= IPOLL_OUT;
	if (revents & POLLERR)eventx |= IPOLL_ERR;

	n = pfd->fd;
	if (ps->fv.fds[n].fd < 0) eventx = 0;
	eventx &= ps->fv.fds[n].mask;

	if (fd) *fd = n;
	if (event) *event = eventx;
	if (user) *user = ps->fv.fds[n].user;

	return 0;
}


#endif


/*===================================================================*/
/* POLL DRIVER - KEVENT                                              */
/*===================================================================*/

#ifdef IHAVE_KEVENT
#include <sys/event.h>
#include <stdio.h>
#include <assert.h>

static int ipk_startup(void);
static int ipk_shutdown(void);
static int ipk_init_pd(ipolld ipd, int param);
static int ipk_destroy_pd(ipolld ipd);
static int ipk_poll_add(ipolld ipd, int fd, int mask, void *user);
static int ipk_poll_del(ipolld ipd, int fd);
static int ipk_poll_set(ipolld ipd, int fd, int mask);
static int ipk_poll_wait(ipolld ipd, int timeval);
static int ipk_poll_event(ipolld ipd, int *fd, int *event, void **user);

/* kevent device structure */
typedef struct
{
	struct IPOLLFV fv;
	int kqueue;
	int num_fd;
	int num_chg;
	int max_fd;
	int max_chg;
	int results;
	int cur_res;
	int usr_len;
	struct kevent *mchange;
	struct kevent *mresult;
	long   stimeval;
	struct timespec stimespec;
	struct IPVECTOR vchange;
	struct IPVECTOR vresult;
}	IPD_KQUEUE;

/* kevent poll descriptor */
struct IPOLL_DRIVER IPOLL_KEVENT = {
	sizeof (IPD_KQUEUE),	
	IDEVICE_KQUEUE,
	100,
	"KQUEUE",
	ipk_startup,
	ipk_shutdown,
	ipk_init_pd,
	ipk_destroy_pd,
	ipk_poll_add,
	ipk_poll_del,
	ipk_poll_set,
	ipk_poll_wait,
	ipk_poll_event
};


#ifdef PSTRUCT
#undef PSTRUCT
#endif

#define PSTRUCT IPD_KQUEUE

/* kevent startup */
static int ipk_startup(void)
{
	return 0;
}

/* kevent shutdown */
static int ipk_shutdown(void)
{
	return 0;
}

/* kevent grow vector */
static int ipk_grow(PSTRUCT *ps, int size_fd, int size_chg);

/* kevent init descriptor */
static int ipk_init_pd(ipolld ipd, int param)
{
	PSTRUCT *ps = PDESC(ipd);
	ps->kqueue = kqueue();
	if (ps->kqueue < 0) return -1;

	ipv_init(&ps->vchange);
	ipv_init(&ps->vresult);
	ipoll_fvinit(&ps->fv);

	ps->max_fd = 0;
	ps->max_chg= 0;
	ps->num_fd = 0;
	ps->num_chg= 0;
	ps->usr_len = 0;
	ps->stimeval = -1;
	param = param;

	if (ipk_grow(ps, 4, 4)) {
		ipoll_fvdestroy(&ps->fv);
		ipk_destroy_pd(ipd);
		return -3;
	}

	return 0;
}

/* kevent destroy descriptor */
static int ipk_destroy_pd(ipolld ipd)
{
	PSTRUCT *ps = PDESC(ipd);

	ipv_destroy(&ps->vchange);
	ipv_destroy(&ps->vresult);
	ipoll_fvdestroy(&ps->fv);

	if (ps->kqueue >= 0) close(ps->kqueue);
	ps->kqueue = -1;

	return 0;
}

/* kevent grow vector */
static int ipk_grow(PSTRUCT *ps, int size_fd, int size_chg)
{
	int r;
	if (size_fd >= 0) {
		r = ipv_resize(&ps->vresult, size_fd * sizeof(struct kevent) * 2);
		ps->mresult = (struct kevent*)ps->vresult.data;
		ps->max_fd = size_fd;
		if (r) return -1;
	}
	if (size_chg>= 0) {
		r = ipv_resize(&ps->vchange, size_chg * sizeof(struct kevent));
		ps->mchange = (struct kevent*)ps->vchange.data;
		ps->max_chg= size_chg;
		if (r) return -2;
	}
	return 0;
}

/* kevent syscall */
static int ipk_poll_kevent(ipolld ipd, int fd, int filter, int flag)
{
	PSTRUCT *ps = PDESC(ipd);
	struct kevent *ke;

	if (fd >= ps->usr_len) return -1;
	if (ps->fv.fds[fd].fd < 0) return -2;
	if (ps->num_chg >= ps->max_chg)
		if (ipk_grow(ps, -1, ps->max_chg * 2)) return -3;

	ke = &ps->mchange[ps->num_chg++];
	memset(ke, 0, sizeof(struct kevent));
	ke->ident = fd;
	ke->filter = filter;
	ke->flags = flag;

	return 0;
}

/* kevent add file */
static int ipk_poll_add(ipolld ipd, int fd, int mask, void *user)
{
	PSTRUCT *ps = PDESC(ipd);
	int usr_nlen, i, flag;

	if (ps->num_fd >= ps->max_fd) {
		if (ipk_grow(ps, ps->max_fd * 2, -1)) return -1;
	}

	if (fd + 1 >= ps->usr_len) {
		usr_nlen = fd + 128;
		ipoll_fvresize(&ps->fv, usr_nlen);
		for (i = ps->usr_len; i < usr_nlen; i++) {
			ps->fv.fds[i].fd = -1;
			ps->fv.fds[i].mask = 0;
			ps->fv.fds[i].user = NULL;
		}
		ps->usr_len = usr_nlen;
	}

	if (ps->fv.fds[fd].fd >= 0) {
		ps->fv.fds[fd].user = user;
		ipk_poll_set(ipd, fd, mask);
		return 0;
	}

	ps->fv.fds[fd].fd = fd;
	ps->fv.fds[fd].user = user;
	ps->fv.fds[fd].mask = mask;

	flag = (mask & IPOLL_IN)? EV_ENABLE : EV_DISABLE;
	if (ipk_poll_kevent(ipd, fd, EVFILT_READ, EV_ADD | flag)) {
		ps->fv.fds[fd].fd = -1;
		ps->fv.fds[fd].user = NULL;
		ps->fv.fds[fd].mask = 0;
		return -3;
	}
	flag = (mask & IPOLL_OUT)? EV_ENABLE : EV_DISABLE;
	if (ipk_poll_kevent(ipd, fd, EVFILT_WRITE, EV_ADD | flag)) {
		ps->fv.fds[fd].fd = -1;
		ps->fv.fds[fd].user = NULL;
		ps->fv.fds[fd].mask = 0;
		return -4;
	}
	ps->num_fd++;

	return 0;
}

/* kevent delete file */
static int ipk_poll_del(ipolld ipd, int fd)
{
	PSTRUCT *ps = PDESC(ipd);

	if (ps->num_fd <= 0) return -1;
	if (fd >= ps->usr_len) return -2;
	if (ps->fv.fds[fd].fd < 0) return -3;

	if (ipk_poll_kevent(ipd, fd, EVFILT_READ, EV_DELETE | EV_DISABLE)) 
		return -4;
	if (ipk_poll_kevent(ipd, fd, EVFILT_WRITE, EV_DELETE| EV_DISABLE)) 
		return -5;

	ps->num_fd--;
	kevent(ps->kqueue, ps->mchange, ps->num_chg, NULL, 0, 0);
	ps->num_chg = 0;
	ps->fv.fds[fd].fd = -1;
	ps->fv.fds[fd].user = 0;
	ps->fv.fds[fd].mask = 0;

	return 0;
}

/* kevent set event mask */
static int ipk_poll_set(ipolld ipd, int fd, int mask)
{
	PSTRUCT *ps = PDESC(ipd);

	if (fd >= ps->usr_len) return -3;
	if (ps->fv.fds[fd].fd < 0) return -4;

	if (mask & IPOLL_IN) {
		if (ipk_poll_kevent(ipd, fd, EVFILT_READ, EV_ENABLE)) return -1;
	}	else {
		if (ipk_poll_kevent(ipd, fd, EVFILT_READ, EV_DISABLE)) return -2;
	}
	if (mask & IPOLL_OUT) {
		if (ipk_poll_kevent(ipd, fd, EVFILT_WRITE, EV_ENABLE)) return -1;
	}	else {
		if (ipk_poll_kevent(ipd, fd, EVFILT_WRITE, EV_DISABLE)) return -2;
	}
	ps->fv.fds[fd].mask = mask;

	return 0;
}

/* kevent wait events */
static int ipk_poll_wait(ipolld ipd, int timeval)
{
	PSTRUCT *ps = PDESC(ipd);
	struct timespec tm;

	if (timeval != ps->stimeval) {
		ps->stimeval = timeval;
		ps->stimespec.tv_sec = timeval / 1000;
		ps->stimespec.tv_nsec = (timeval % 1000) * 1000000;	
	}
	tm = ps->stimespec;

	ps->results = kevent(ps->kqueue, ps->mchange, ps->num_chg, ps->mresult,
		ps->max_fd * 2, (timeval >= 0) ? &tm : (struct timespec *) 0);
	ps->cur_res = 0;
	ps->num_chg = 0;

	return ps->results;
}

/* kevent query events */
static int ipk_poll_event(ipolld ipd, int *fd, int *event, void **user)
{
	PSTRUCT *ps = PDESC(ipd);
	struct kevent *ke;
	int revent = 0, n;
	if (ps->cur_res >= ps->results) return -1;

	ke = &ps->mresult[ps->cur_res++];
	n = ke->ident;

	if (ke->filter == EVFILT_READ) revent = IPOLL_IN;
	else if (ke->filter == EVFILT_WRITE)revent = IPOLL_OUT;
	else revent = IPOLL_ERR;
	if ((ke->flags & EV_ERROR)) revent = IPOLL_ERR;

	if (ps->fv.fds[n].fd < 0) revent = 0;
	revent &= ps->fv.fds[n].mask;

	if (fd) *fd = n;
	if (event) *event = revent;
	if (user) *user = ps->fv.fds[n].user;

	return 0;
}


#endif


/*===================================================================*/
/* POLL DRIVER - EPOLL                                               */
/*===================================================================*/

#ifdef IHAVE_EPOLL

#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>

#ifndef IEPOLL_LIMIT
#define IEPOLL_LIMIT 20000
#endif

static int ipe_startup(void);
static int ipe_shutdown(void);
static int ipe_init_pd(ipolld ipd, int param);
static int ipe_destroy_pd(ipolld ipd);
static int ipe_poll_add(ipolld ipd, int fd, int mask, void *user);
static int ipe_poll_del(ipolld ipd, int fd);
static int ipe_poll_set(ipolld ipd, int fd, int mask);
static int ipe_poll_wait(ipolld ipd, int timeval);
static int ipe_poll_event(ipolld ipd, int *fd, int *event, void **user);

/* epoll device structure */
typedef struct
{
	struct IPOLLFV fv;
	int epfd;
	int num_fd;
	int max_fd;
	int results;
	int cur_res;
	int usr_len;
	struct epoll_event *mresult;
	struct IPVECTOR vresult;
}	IPD_EPOLL;

/* epoll poll descriptor */
struct IPOLL_DRIVER IPOLL_EPOLL = {
	sizeof (IPD_EPOLL),	
	IDEVICE_EPOLL,
	100,
	"EPOLL",
	ipe_startup,
	ipe_shutdown,
	ipe_init_pd,
	ipe_destroy_pd,
	ipe_poll_add,
	ipe_poll_del,
	ipe_poll_set,
	ipe_poll_wait,
	ipe_poll_event
};


#ifdef PSTRUCT
#undef PSTRUCT
#endif

#define PSTRUCT IPD_EPOLL

/* epoll startup */
static int ipe_startup(void)
{
	int epfd = epoll_create(20);
	if (epfd < 0) return -1000 - errno;
	close(epfd);
	return 0;
}

/* epoll shutdown */
static int ipe_shutdown(void)
{
	return 0;
}

/* epoll init poll descriptor */
static int ipe_init_pd(ipolld ipd, int param)
{
	PSTRUCT *ps = PDESC(ipd);
	ps->epfd = epoll_create(param);
	if (ps->epfd < 0) return -1;

	ipv_init(&ps->vresult);
	ipoll_fvinit(&ps->fv);

	ps->max_fd = 0;
	ps->num_fd = 0;
	ps->usr_len = 0;
	
	if (ipv_resize(&ps->vresult, 4 * sizeof(struct epoll_event))) 
		return -2;
	ps->mresult = (struct epoll_event*)ps->vresult.data;
	ps->max_fd = 4;

	return 0;
}

/* epoll destroy descriptor */
static int ipe_destroy_pd(ipolld ipd)
{
	PSTRUCT *ps = PDESC(ipd);
	ipv_destroy(&ps->vresult);
	ipoll_fvdestroy(&ps->fv);

	if (ps->epfd >= 0) close(ps->epfd);
	ps->epfd = -1;
	return 0;
}

/* epoll add file */
static int ipe_poll_add(ipolld ipd, int fd, int mask, void *user)
{
	PSTRUCT *ps = PDESC(ipd);
	int usr_nlen, i;
	struct epoll_event ee;

	if (ps->num_fd >= ps->max_fd) {
		i = (ps->max_fd <= 0)? 4 : ps->max_fd * 2;
		if (ipv_resize(&ps->vresult, i * sizeof(struct epoll_event))) 
			return -1;
		ps->mresult = (struct epoll_event*)ps->vresult.data;
		ps->max_fd = i;
	}
	if (fd >= ps->usr_len) {
		usr_nlen = fd + 128;
		ipoll_fvresize(&ps->fv, usr_nlen);
		for (i = ps->usr_len; i < usr_nlen; i++) {
			ps->fv.fds[i].fd = -1;
			ps->fv.fds[i].user = NULL;
			ps->fv.fds[i].mask = 0;
		}
		ps->usr_len = usr_nlen;
	}
	if (ps->fv.fds[fd].fd >= 0) {
		ps->fv.fds[fd].user = user;
		ipe_poll_set(ipd, fd, mask);
		return 0;
	}
	ps->fv.fds[fd].fd = fd;
	ps->fv.fds[fd].user = user;
	ps->fv.fds[fd].mask = mask;

	ee.events = 0;
	ee.data.fd = fd;

	if (mask & IPOLL_IN) ee.events |= EPOLLIN | EPOLLERR | EPOLLHUP;
	if (mask & IPOLL_OUT) ee.events |= EPOLLOUT | EPOLLERR;
	if (mask & IPOLL_ERR) ee.events |= EPOLLERR;

	if (epoll_ctl(ps->epfd, EPOLL_CTL_ADD, fd, &ee)) {
		ps->fv.fds[fd].fd = -1;
		ps->fv.fds[fd].user = NULL;
		ps->fv.fds[fd].mask = 0;
		return -3;
	}
	ps->num_fd++;

	return 0;
}

/* epoll delete file */
static int ipe_poll_del(ipolld ipd, int fd)
{
	PSTRUCT *ps = PDESC(ipd);
	struct epoll_event ee;

	if (ps->num_fd <= 0) return -1;
	if (ps->fv.fds[fd].fd <  0) return -2;

	ee.events = 0;
	ee.data.fd = fd;

	epoll_ctl(ps->epfd, EPOLL_CTL_DEL, fd, &ee);
	ps->num_fd--;
	ps->fv.fds[fd].fd = -1;
	ps->fv.fds[fd].user = NULL;
	ps->fv.fds[fd].mask = 0;

	return 0;
}

/* epoll set event mask */
static int ipe_poll_set(ipolld ipd, int fd, int mask)
{
	PSTRUCT *ps = PDESC(ipd);
	struct epoll_event ee;
	int retval;

	ee.events = 0;
	ee.data.fd = fd;

	if (fd >= ps->usr_len) return -1;
	if (ps->fv.fds[fd].fd < 0) return -2;

	ps->fv.fds[fd].mask = 0;

	if (mask & IPOLL_IN) {
		ee.events |= EPOLLIN | EPOLLERR | EPOLLHUP;
		ps->fv.fds[fd].mask |= IPOLL_IN;
	}
	if (mask & IPOLL_OUT) {
		ee.events |= EPOLLOUT | EPOLLERR;
		ps->fv.fds[fd].mask |= IPOLL_OUT;
	}
	if (mask & IPOLL_ERR) {
		ee.events |= EPOLLERR;
		ps->fv.fds[fd].mask |= IPOLL_ERR;
	}

	retval = epoll_ctl(ps->epfd, EPOLL_CTL_MOD, fd, &ee);
	if (retval) return -10000 + retval;

	return 0;
}

/* epoll wait */
static int ipe_poll_wait(ipolld ipd, int timeval)
{
	PSTRUCT *ps = PDESC(ipd);

	ps->results = epoll_wait(ps->epfd, ps->mresult, 
		ps->max_fd * 2, timeval);
	ps->cur_res = 0;

	return ps->results;
}

/* epoll query event */
static int ipe_poll_event(ipolld ipd, int *fd, int *event, void **user)
{
	PSTRUCT *ps = PDESC(ipd);
	struct epoll_event *ee;
	int revent = 0, n;

	if (ps->cur_res >= ps->results) return -1;

	ee = &ps->mresult[ps->cur_res++];
	n = ee->data.fd;
	if (fd) *fd = n;

	if (ee->events & EPOLLIN) revent |= IPOLL_IN;
	if (ee->events & EPOLLOUT) revent |= IPOLL_OUT;
	if (ee->events & (EPOLLERR | EPOLLHUP)) revent |= IPOLL_ERR; 

	if (ps->fv.fds[n].fd < 0) revent = 0;
	revent &= ps->fv.fds[n].mask;

	if (event) *event = revent;
	if (user) *user = ps->fv.fds[n].user;

	return 0;
}


#endif

