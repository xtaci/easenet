/**********************************************************************
 *
 * inetcode.c - core interface of socket operation
 *
 * for more information, please see the readme file
 *
 **********************************************************************/

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


/*===================================================================*/
/* Network Information                                               */
/*===================================================================*/
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


/*===================================================================*/
/* ITMCLIENT                                                         */
/*===================================================================*/
struct ITMCLIENT
{
	IUINT32 time;
	int sock;
	int state;
	long hid;
	long tag;
	int endian;
	int error;
	char *buffer;
	struct sockaddr local;
	struct IMSTREAM sendmsg;
	struct IMSTREAM recvmsg;
};

void itmc_init(struct ITMCLIENT *client, struct IMEMNODE *nodes);
void itmc_destroy(struct ITMCLIENT *client);

int itmc_connect(struct ITMCLIENT *client, const struct sockaddr *addr);
int itmc_assign(struct ITMCLIENT *client, int sock);
int itmc_process(struct ITMCLIENT *client);
int itmc_close(struct ITMCLIENT *client);
int itmc_status(struct ITMCLIENT *client);
int itmc_nodelay(struct ITMCLIENT *client, int nodelay);
int itmc_dsize(const struct ITMCLIENT *client);

int itmc_send(struct ITMCLIENT *client, const void *data, int size);
int itmc_recv(struct ITMCLIENT *client, void *data, int size);


int itmc_vecsend(struct ITMCLIENT *client, const void *vecptr[], 
	int veclen[], int count);



/*===================================================================*/
/* ITMHOST                                                           */
/*===================================================================*/
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
	int endian;
	int timeout;
	int needfree;
};

void itmh_init(struct ITMHOST *host, struct IMEMNODE *cache);
void itmh_destroy(struct ITMHOST *host);

int itmh_startup(struct ITMHOST *host, int port);
int itmh_shutdown(struct ITMHOST *host);

void itmh_process(struct ITMHOST *host);

void itmh_send(struct ITMHOST *host, long hid, const void *data, long size);
void itmh_close(struct ITMHOST *host, long hid, int reason);
void itmh_settag(struct ITMHOST *host, long hid, long tag);
long itmh_gettag(struct ITMHOST *host, long hid);
void itmh_nodelay(struct ITMHOST *host, long hid, int nodelay);

long itmh_head(struct ITMHOST *host);
long itmh_next(struct ITMHOST *host, long hid);


#define ITME_NEW	0	/* wparam=hid, lparam=-1, data=sockaddr */
#define ITME_DATA	1	/* wparam=hid, lparam=tag, data=recvdata */
#define ITME_LEAVE	2	/* wparam=hid, lparam=tag, data=reason */
#define ITME_TIMER  3   /* wparam=-1, lparam=-1 */

long itmh_read(struct ITMHOST *host, int *msg, long *wparam, long *lparam,
	void *data, long size);



/*===================================================================*/
/* IHOSTUDP                                                          */
/*===================================================================*/
struct IHOSTUDP
{
	int sock;
	int port;
	int state;
	char *buffer;
	int needsend;
	int needrecv;
	int needlock;
	int MTU;
	IMUTEX_TYPE lock;
	struct sockaddr local;
	struct IMEMNODE *cache;
	struct IMSTREAM recvbuf;
	struct IMSTREAM sendbuf;
};


void ihu_init(struct IHOSTUDP *uhost, struct IMEMNODE *cache);
void ihu_destroy(struct IHOSTUDP *uhost);

int ihu_startup(struct IHOSTUDP *uhost, int port);
int ihu_shutdown(struct IHOSTUDP *uhost);

void ihu_process(struct IHOSTUDP *uhost);

int ihu_sendto(struct IHOSTUDP *uhost, struct sockaddr *remote, 
	const void *buf, int size);

int ihu_recvfrom(struct IHOSTUDP *uhost, struct sockaddr *remote,
	void *buf, int size);


#ifdef __cplusplus
}
#endif



#endif



