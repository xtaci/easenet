/**********************************************************************
 *
 * inetbase.h - basic interface of socket operation
 *
 * for more information, please see the readme file
 *
 **********************************************************************/

#ifndef __INETBASE_H__
#define __INETBASE_H__


/*-------------------------------------------------------------------*/
/* C99 Compatible                                                    */
/*-------------------------------------------------------------------*/
#ifdef _POSIX_C_SOURCE
#if _POSIX_C_SOURCE < 200112L
#undef _POSIX_C_SOURCE
#endif
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#ifdef _GNU_SOURCE
#undef _GNU_SOURCE
#endif

#ifdef _BSD_SOURCE
#undef _BSD_SOURCE
#endif

#ifdef __BSD_VISIBLE
#undef __BSD_VISIBLE
#endif

#define _GNU_SOURCE 1
#define _BSD_SOURCE 1
#define __BSD_VISIBLE 1


/*-------------------------------------------------------------------*/
/* Unix Platform                                                     */
/*-------------------------------------------------------------------*/
#if defined(__unix) || defined(unix) || defined(__MACH__)
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>


#ifndef __unix
#define __unix
#endif

#if defined(__MACH__) && (!defined(_SOCKLEN_T)) && (!defined(HAVE_SOCKLEN))
typedef int socklen_t;
#endif

#include <errno.h>

#define IESOCKET		-1
#define IEAGAIN			EAGAIN

/*-------------------------------------------------------------------*/
/* Windows Platform                                                  */
/*-------------------------------------------------------------------*/
#elif defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
#if ((!defined(_M_PPC)) && (!defined(_M_PPC_BE)) && (!defined(_XBOX)))
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#ifndef WIN32_LEAN_AND_MEAN  
#define WIN32_LEAN_AND_MEAN  
#endif
#include <windows.h>
#include <winsock2.h>
#else
#ifndef _XBOX
#define _XBOX
#endif
#include <xtl.h>
#include <winsockx.h>
#endif

#include <errno.h>

#define IESOCKET		SOCKET_ERROR
#define IEAGAIN			WSAEWOULDBLOCK

#ifndef _WIN32
#define _WIN32
#endif

typedef int socklen_t;
typedef SOCKET Socket;

#ifndef EWOULDBLOCK
#define EWOULDBLOCK             WSAEWOULDBLOCK
#endif

#ifndef EAGAIN
#define EAGAIN                  WSAEWOULDBLOCK
#endif

#define EINPROGRESS             WSAEINPROGRESS
#define EALREADY                WSAEALREADY
#define ENOTSOCK                WSAENOTSOCK
#define EDESTADDRREQ            WSAEDESTADDRREQ
#define EMSGSIZE                WSAEMSGSIZE
#define EPROTOTYPE              WSAEPROTOTYPE
#define ENOPROTOOPT             WSAENOPROTOOPT
#define EPROTONOSUPPORT         WSAEPROTONOSUPPORT
#define ESOCKTNOSUPPORT         WSAESOCKTNOSUPPORT
#define EOPNOTSUPP              WSAEOPNOTSUPP
#define EPFNOSUPPORT            WSAEPFNOSUPPORT
#define EAFNOSUPPORT            WSAEAFNOSUPPORT
#define EADDRINUSE              WSAEADDRINUSE
#define EADDRNOTAVAIL           WSAEADDRNOTAVAIL
#define ENETDOWN                WSAENETDOWN
#define ENETUNREACH             WSAENETUNREACH
#define ENETRESET               WSAENETRESET
#define ECONNABORTED            WSAECONNABORTED
#define ECONNRESET              WSAECONNRESET
#define ENOBUFS                 WSAENOBUFS
#define EISCONN                 WSAEISCONN
#define ENOTCONN                WSAENOTCONN
#define ESHUTDOWN               WSAESHUTDOWN
#define ETOOMANYREFS            WSAETOOMANYREFS
#define ETIMEDOUT               WSAETIMEDOUT
#define ECONNREFUSED            WSAECONNREFUSED
#define ELOOP                   WSAELOOP
#define EHOSTDOWN               WSAEHOSTDOWN
#define EHOSTUNREACH            WSAEHOSTUNREACH
#define EPROCLIM                WSAEPROCLIM
#define EUSERS                  WSAEUSERS
#define EDQUOT                  WSAEDQUOT
#define ESTALE                  WSAESTALE
#define EREMOTE                 WSAEREMOTE

#else
#error Unknow platform, only support unix and win32
#endif

#ifndef EINVAL
#define EINVAL          22
#endif

#ifdef IHAVE_CONFIG_H
#include "config.h"
#endif

#if defined(i386) || defined(__i386__) || defined(__i386) || \
	defined(_M_IX86) || defined(_X86_) || defined(__THW_INTEL)
	#ifndef __i386__
	#define __i386__
	#endif
#endif


/*===================================================================*/
/* Global Macro Definition                                           */
/*===================================================================*/
#define ISOCK_NOBLOCK	1		/* flag - non-block       */
#define ISOCK_REUSEADDR	2		/* flag - reuse address   */
#define ISOCK_NODELAY	3		/* flag - no delay        */
#define ISOCK_NOPUSH	4		/* flag - no push         */

#define ISOCK_ERECV		1		/* event - recv           */
#define ISOCK_ESEND		2		/* event - send           */
#define ISOCK_ERROR		4		/* event - error          */

#if (defined(__BORLANDC__) || defined(__WATCOMC__))
#pragma warn -8002  
#pragma warn -8004  
#pragma warn -8008  
#pragma warn -8012
#pragma warn -8027
#pragma warn -8057  
#pragma warn -8066 
#endif

#ifndef ICLOCKS_PER_SEC
#ifdef CLK_TCK
#define ICLOCKS_PER_SEC CLK_TCK
#else
#define ICLOCKS_PER_SEC 1000000
#endif
#endif

#ifndef __IINT32_DEFINED
#define __IINT32_DEFINED
typedef long IINT32;
#endif

#ifndef __IUINT32_DEFINED
#define __IUINT32_DEFINED
typedef unsigned long IUINT32;
#endif

#ifndef __IINT64_DEFINED
#define __IINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef __int64 IINT64;
#else
typedef long long IINT64;
#endif
#endif

#ifndef __IUINT64_DEFINED
#define __IUINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef unsigned __int64 IUINT64;
#else
typedef unsigned long long IUINT64;
#endif
#endif


/*====================================================================*/
/* IULONG/ILONG (ensure sizeof(iulong) == sizeof(void*))              */
/*====================================================================*/
#ifndef __IULONG_DEFINED
#define __IULONG_DEFINED
#if defined(WIN64) || defined(_WIN64)		/* LLP64 mode */
#ifdef _MSC_VER
typedef unsigned __int64 iulong;
typedef __int64 ilong;
#else										/* LP64 or 32/16 mode */
typedef unsigned long long iulong;
typedef long long ilong;
#endif
#else
typedef unsigned long iulong;
typedef long ilong;
#endif
#endif


#ifdef __cplusplus
extern "C" {
#endif


/*===================================================================*/
/* Cross-Platform Time Interface                                     */
/*===================================================================*/

/* sleep in millisecond */
void isleep(unsigned long millisecond);

/* get system time of day */
void itimeofday(long *sec, long *usec);

/* get clock in millisecond */
unsigned long iclock(void);

/* get clock in millisecond 64 */
IINT64 iclock64(void);

/* global millisecond clock value, updated by itimeofday */
volatile extern IINT64 itimeclock;


/*===================================================================*/
/* Cross-Platform Threading Interface                                */
/*===================================================================*/

/* thread entry */
typedef void (*ITHREADPROC)(void *);

/* create thread */
int ithread_create(ilong *id, ITHREADPROC fun, const void *attr, void *args);

/* exit thread */
void ithread_exit(long retval);

/* join thread */
int ithread_join(ilong id);

/* detach thread */
int ithread_detach(ilong id);

/* kill thread */
int ithread_kill(ilong id);


/*===================================================================*/
/* Cross-Platform Mutex Interface                                    */
/*===================================================================*/
#ifndef IMUTEX_TYPE

#if (defined(WIN32) || defined(_WIN32))
#if ((!defined(_M_PPC)) && (!defined(_M_PPC_BE)) && (!defined(_XBOX)))
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#ifndef WIN32_LEAN_AND_MEAN  
#define WIN32_LEAN_AND_MEAN  
#endif
#include <windows.h>
#else
#ifndef _XBOX
#define _XBOX
#endif
#include <xtl.h>
#endif

#define IMUTEX_TYPE			CRITICAL_SECTION
#define IMUTEX_INIT(m)		InitializeCriticalSection((CRITICAL_SECTION*)(m))
#define IMUTEX_DESTROY(m)	DeleteCriticalSection((CRITICAL_SECTION*)(m))
#define IMUTEX_LOCK(m)		EnterCriticalSection((CRITICAL_SECTION*)(m))
#define IMUTEX_UNLOCK(m)	LeaveCriticalSection((CRITICAL_SECTION*)(m))

#elif defined(__unix) || defined(unix) || defined(__MACH__)
#include <unistd.h>
#include <pthread.h>
#define IMUTEX_TYPE			pthread_mutex_t
#define IMUTEX_INIT(m)		pthread_mutex_init((pthread_mutex_t*)(m), 0)
#define IMUTEX_DESTROY(m)	pthread_mutex_destroy((pthread_mutex_t*)(m))
#define IMUTEX_LOCK(m)		pthread_mutex_lock((pthread_mutex_t*)(m))
#define IMUTEX_UNLOCK(m)	pthread_mutex_unlock((pthread_mutex_t*)(m))

#else
#define IMUTEX_TYPE			int
#define IMUTEX_INIT(m)		
#define IMUTEX_DESTROY(m)	
#define IMUTEX_LOCK(m)		
#define IMUTEX_UNLOCK(m)	
#endif

#endif



/*===================================================================*/
/* Cross-Platform Socket Interface                                   */
/*===================================================================*/

/* create socket */
int isocket(int family, int type, int protocol);

/* close socket */
int iclose(int sock);

/* connect to */
int iconnect(int sock, const struct sockaddr *addr);

/* shutdown */
int ishutdown(int sock, int mode);

/* bind */
int ibind(int sock, const struct sockaddr *addr);

/* listen */
int ilisten(int sock, int count);

/* accept */
int iaccept(int sock, struct sockaddr *addr);

/* get errno */
int ierrno(void);

/* send */
int isend(int sock, const void *buf, long size, int mode);

/* receive */
int irecv(int sock, void *buf, long size, int mode);

/* sendto */
int isendto(int sock, const void *buf, long size, int mode, 
	const struct sockaddr *addr);

/* recvfrom */
int irecvfrom(int sock, void *buf, long size, int mode, 
	struct sockaddr *addr);

/* ioctl */
int iioctl(int sock, long cmd, unsigned long *argp);

/* set socket option */
int isetsockopt(int sock, int level, int optname, const char *optval, 
	int optlen);

/* get socket option */
int igetsockopt(int sock, int level, int optname, char *optval, 
	int *optlen);

/* get socket name */
int isockname(int sock, struct sockaddr *addr);

/* get peer name */
int ipeername(int sock, struct sockaddr *addr);


/*===================================================================*/
/* Basic Function Definition                                         */
/*===================================================================*/

/* enable option */
int ienable(int fd, int mode);

/* disable option */
int idisable(int fd, int mode);

/* poll event */
int ipollfd(int sock, int event, long millsec);

/* send all data */
int isendall(int sock, const void *buf, long size);

/* try to receive all data */
int irecvall(int sock, void *buf, long size);

/* format error string */
char *ierrstr(int errnum, char *msg, int size);

/* get host address */
int igethostaddr(struct in_addr *addrs, int count);

/* setup sockaddr */
struct sockaddr* isockaddr_set(struct sockaddr *a, unsigned long ip, int p);

/* set address of sockaddr */
void isockaddr_set_ip(struct sockaddr *a, unsigned long ip);

/* get address of sockaddr */
unsigned long isockaddr_get_ip(const struct sockaddr *a);

/* set port of sockaddr */
void isockaddr_set_port(struct sockaddr *a, int port);

/* get port of sockaddr */
int isockaddr_get_port(struct sockaddr *a);

/* set family */
void isockaddr_set_family(struct sockaddr *a, int family);

/* get family */
int isockaddr_get_family(struct sockaddr *a);

/* set text to ip */
int isockaddr_set_ip_text(struct sockaddr *a, const char *text);

/* make up address */
struct sockaddr *isockaddr_makeup(struct sockaddr *a, const char *ip, int p);

/* convert address to text */
char *isockaddr_str(const struct sockaddr *a, char *text);

/* compare two addresses */
int isockaddr_cmp(const struct sockaddr *a, const struct sockaddr *b);



/*===================================================================*/
/* INLINE COMPATIBLE                                                 */
/*===================================================================*/
#ifndef INLINE
#ifdef __GNUC__

#if __GNUC_MINOR__ >= 1  && __GNUC_MINOR__ < 4
#define INLINE         __inline__ __attribute__((always_inline))
#else
#define INLINE         __inline__
#endif

#elif (defined(_MSC_VER) || defined(__BORLANDC__) || defined(__WATCOMC__))
#define INLINE __inline
#else
#define INLINE 
#endif
#endif

#ifndef inline
#define inline INLINE
#endif


/*===================================================================*/
/* Simple Assistant Function                                         */
/*===================================================================*/

/* start network */
int inet_init(void);

/* open a binded udp socket */
int inet_open_port(unsigned short port, unsigned long ip, int noblock);

/* set recv buf and send buf */
int inet_set_bufsize(int sock, long recvbuf_size, long sendbuf_size);



/*===================================================================*/
/* Cross-Platform Poll Interface                                     */
/*===================================================================*/
#define IDEVICE_AUTO		0
#define IDEVICE_SELECT		1
#define IDEVICE_POLL		2
#define IDEVICE_KQUEUE		3
#define IDEVICE_WINCP		4
#define IDEVICE_EPOLL		5
#define IDEVICE_DEVPOLL		6
#define IDEVICE_RTSIG		7

#ifndef IPOLL_IN
#define IPOLL_IN	1
#endif

#ifndef IPOLL_OUT
#define IPOLL_OUT	2
#endif

#ifndef IPOLL_ERR
#define IPOLL_ERR	4
#endif

typedef void * ipolld;

/* init poll device */
int ipoll_init(int device);

/* quit poll device */
int ipoll_quit(void);

/* create poll descriptor */
int ipoll_create(ipolld *ipd, int param);

/* delete poll descriptor */
int ipoll_delete(ipolld ipd);

/* add file/socket into poll descriptor */
int ipoll_add(ipolld ipd, int fd, int mask, void *udata);

/* delete file/socket from poll descriptor */
int ipoll_del(ipolld ipd, int fd);

/* set file event mask in a poll descriptor */
int ipoll_set(ipolld ipd, int fd, int mask);

/* wait for events coming */
int ipoll_wait(ipolld ipd, int millisecond);

/* query one event: loop call it until it returns non-zero */
int ipoll_event(ipolld ipd, int *fd, int *event, void **udata);




#ifdef __cplusplus
}
#endif


#endif



