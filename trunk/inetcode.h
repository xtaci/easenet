//=====================================================================
//
// inetcode.c - core interface of socket operation
//
// NOTE:
// for more information, please see the readme file
// 
//=====================================================================

#ifndef __INETCODE_H__
#define __INETCODE_H__

#include "inetbase.h"
#include "imemdata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


//---------------------------------------------------------------------
// Network Information
//---------------------------------------------------------------------
#define IMAX_HOSTNAME	256
#define IMAX_ADDRESS	64

/* host name */
extern char ihostname[];

/* host address list */
extern struct in_addr ihost_addr[];

/* host ip address string list */
extern char *ihost_ipstr[];

/* host names */
extern char *ihost_names[];

/* host address count */
extern int ihost_addr_num;	

/* refresh address list */
int inet_updateaddr(int resolvname);

/* create socketpair */
int inet_socketpair(int fds[2]);

/* keepalive option */
int inet_set_keepalive(int sock, int keepcnt, int keepidle, int keepintvl);


//---------------------------------------------------------------------
// ITMCLIENT
//---------------------------------------------------------------------
struct ITMCLIENT
{
	IUINT32 time;
	int sock;
	int state;
	long hid;
	long tag;
	int error;
	int header;
	char *buffer;
	int rc4_send_x;
	int rc4_send_y;
	int rc4_recv_x;
	int rc4_recv_y;
	unsigned char *rc4_send_box;
	unsigned char *rc4_recv_box;
	struct sockaddr local;
	struct IMSTREAM sendmsg;
	struct IMSTREAM recvmsg;
};

#ifndef ITMH_WORDLSB
#define ITMH_WORDLSB		0		// 头部标志：2字节LSB
#define ITMH_WORDMSB		1		// 头部标志：2字节MSB
#define ITMH_DWORDLSB		2		// 头部标志：4字节LSB
#define ITMH_DWORDMSB		3		// 头部标志：4字节MSB
#define ITMH_BYTELSB		4		// 头部标志：单字节LSB
#define ITMH_BYTEMSB		5		// 头部标志：单字节MSB
#define ITMH_EWORDLSB		6		// 头部标志：2字节LSB（不包含自己）
#define ITMH_EWORDMSB		7		// 头部标志：2字节MSB（不包含自己）
#define ITMH_EDWORDLSB		8		// 头部标志：4字节LSB（不包含自己）
#define ITMH_EDWORDMSB		9		// 头部标志：4字节MSB（不包含自己）
#define ITMH_EBYTELSB		10		// 头部标志：单字节LSB（不包含自己）
#define ITMH_EBYTEMSB		11		// 头部标志：单字节MSB（不包含自己）
#define ITMH_DWORDMASK		12		// 头部标志：4字节LSB（包含自己和掩码）
#endif

#define ITMC_STATE_CLOSED		0	// 状态：关闭
#define ITMC_STATE_CONNECTING	1	// 状态：连接中
#define ITMC_STATE_ESTABLISHED	2	// 状态：连接建立


void itmc_init(struct ITMCLIENT *client, struct IMEMNODE *nodes, int header);
void itmc_destroy(struct ITMCLIENT *client);

int itmc_connect(struct ITMCLIENT *client, const struct sockaddr *addr);
int itmc_assign(struct ITMCLIENT *client, int sock);
int itmc_process(struct ITMCLIENT *client);
int itmc_close(struct ITMCLIENT *client);
int itmc_status(struct ITMCLIENT *client);
int itmc_nodelay(struct ITMCLIENT *client, int nodelay);
int itmc_dsize(const struct ITMCLIENT *client);

int itmc_send(struct ITMCLIENT *client, const void *ptr, int size, int mask);
int itmc_recv(struct ITMCLIENT *client, void *ptr, int size);


int itmc_vsend(struct ITMCLIENT *client, const void *vecptr[], 
	int veclen[], int count, int mask);

int itmc_wait(struct ITMCLIENT *client, int millisec);

void itmc_rc4_set_skey(struct ITMCLIENT *client, 
	const unsigned char *key, int keylen);

void itmc_rc4_set_rkey(struct ITMCLIENT *client, 
	const unsigned char *key, int keylen);


//---------------------------------------------------------------------
// ITMHOST
//---------------------------------------------------------------------
struct ITMHOST
{
	struct IMEMNODE *nodes;
	struct IMEMNODE *cache;
	struct IMSTREAM event;
	struct sockaddr local;
	char *buffer;
	int count;
	int sock;
	int port;
	int state;
	int index;
	int limit;
	int header;
	int timeout;
	int needfree;
};

void itms_init(struct ITMHOST *host, struct IMEMNODE *cache, int header);
void itms_destroy(struct ITMHOST *host);

int itms_startup(struct ITMHOST *host, int port);
int itms_shutdown(struct ITMHOST *host);

void itms_process(struct ITMHOST *host);

void itms_send(struct ITMHOST *host, long hid, const void *data, long size);
void itms_close(struct ITMHOST *host, long hid, int reason);
void itms_settag(struct ITMHOST *host, long hid, long tag);
long itms_gettag(struct ITMHOST *host, long hid);
void itms_nodelay(struct ITMHOST *host, long hid, int nodelay);

long itms_head(struct ITMHOST *host);
long itms_next(struct ITMHOST *host, long hid);


#define ITME_NEW	0	/* wparam=hid, lparam=-1, data=sockaddr */
#define ITME_DATA	1	/* wparam=hid, lparam=tag, data=recvdata */
#define ITME_LEAVE	2	/* wparam=hid, lparam=tag, data=reason */
#define ITME_TIMER  3   /* wparam=-1, lparam=-1 */

long itms_read(struct ITMHOST *host, int *msg, long *wparam, long *lparam,
	void *data, long size);



//---------------------------------------------------------------------
// Utilies Interface
//---------------------------------------------------------------------



#ifdef __cplusplus
}
#endif



#endif



