/**********************************************************************
 *
 * inetcode.c - core interface of socket operation
 *
 * for more information, please see the readme file
 *
 **********************************************************************/

#include "inetcode.h"
#include <assert.h>


/*===================================================================*/
/* Network Information                                               */
/*===================================================================*/

/* host name */
char ihostname[IMAX_HOSTNAME];

/* host address list */
struct in_addr ihost_addr[IMAX_ADDRESS];

/* host ip address string list */
char *ihost_ipstr[IMAX_ADDRESS];

/* host names */
char *ihost_names[IMAX_ADDRESS];

/* host address count */
int ihost_addr_num = 0;	

/* refresh address list */
int inet_updateaddr(int resolvname)
{
	static int inited = 0;
	unsigned char *bytes;
	int count, i, j;
	int ip4[4];

	if (inited == 0) {
		for (i = 0; i < IMAX_ADDRESS; i++) {
			ihost_ipstr[i] = (char*)malloc(16);
			ihost_names[i] = (char*)malloc(64);
			assert(ihost_ipstr[i]);
			assert(ihost_names[i]);
		}
		inet_init();
		#ifndef _XBOX
		if (gethostname(ihostname, IMAX_HOSTNAME)) {
			strcpy(ihostname, "unknowhost");
		}
		#else
		strcpy(ihostname, "unknowhost");
		#endif
		inited = 1;
	}

	ihost_addr_num = igethostaddr(ihost_addr, IMAX_ADDRESS);
	count = ihost_addr_num;

	for (i = 0; i < count; i++) {
		bytes = (unsigned char*)&(ihost_addr[i].s_addr);
		for (j = 0; j < 4; j++) ip4[j] = bytes[j];
		sprintf(ihost_ipstr[i], "%d.%d.%d.%d", 
			ip4[0], ip4[1], ip4[2], ip4[3]);
		strcpy(ihost_names[i], ihost_ipstr[i]);
	}

	#ifndef _XBOX
	for (i = 0; i < count; i++) {
		if (resolvname) {
			struct hostent *ent;
			ent = gethostbyaddr((const char*)&ihost_addr[i], 4, AF_INET);
		}
	}
	#endif

	return 0;
}


// socketpair
static int inet_sockpair_imp(int fds[2])
{
	struct sockaddr_in addr1 = { 0 };
	struct sockaddr_in addr2;
	int sock[2] = { -1, -1 };
	int listener;

	if ((listener = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		return -1;
	
	addr1.sin_family = AF_INET;
	addr1.sin_port = 0;
	addr1.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (ibind(listener, (struct sockaddr*)&addr1))
		goto failed;

	if (isockname(listener, (struct sockaddr*)&addr1))
		goto failed;
	
	if (listen(listener, 1))
		goto failed;

	if ((sock[0] = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		goto failed;

	if (iconnect(sock[0], (struct sockaddr*)&addr1))
		goto failed;

	if ((sock[1] = accept(listener, 0, 0)) < 0)
		goto failed;

	if (ipeername(sock[0], (struct sockaddr*)&addr1))
		goto failed;

	if (isockname(sock[1], (struct sockaddr*)&addr2))
		goto failed;

	if (addr1.sin_addr.s_addr != addr2.sin_addr.s_addr ||
		addr1.sin_port != addr2.sin_port)
		goto failed;

	iclose(listener);
	fds[0] = sock[0];
	fds[1] = sock[1];

	return 0;

failed:
	iclose(listener);
	if (sock[0] >= 0) iclose(sock[0]);
	if (sock[1] >= 0) iclose(sock[1]);
	return -1;
}

int inet_socketpair(int fds[2])
{
	if (inet_sockpair_imp(fds) == 0) return 0;
	if (inet_sockpair_imp(fds) == 0) return 0;
	if (inet_sockpair_imp(fds) == 0) return 0;
	return -1;
}


/*-------------------------------------------------------------------*/
/* set keepalive                                                     */
/*-------------------------------------------------------------------*/
int inet_set_keepalive(int sock, int keepcnt, int keepidle, int keepintvl)
{
	int enable = (keepcnt < 0 || keepidle < 0 || keepintvl < 0)? 0 : 1;

#if (defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(WIN64))
	#define _SIO_KEEPALIVE_VALS _WSAIOW(IOC_VENDOR, 4)
	unsigned long keepalive[3], oldkeep[3], retval;
	OSVERSIONINFO info;
	int candoit = 0;

	info.dwOSVersionInfoSize = sizeof(info);
	GetVersionEx(&info);

	if (info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		if ((info.dwMajorVersion == 5 && info.dwMinorVersion >= 1) ||
			(info.dwMajorVersion >= 6)) {
			candoit = 1;
		}
	}

	retval = 1;
	isetsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&retval, 
		sizeof(retval));

	if (candoit) {
		int ret = 0;
		keepalive[0] = enable? 1 : 0;
		keepalive[1] = ((unsigned long)keepidle) * 1000;
		keepalive[2] = ((unsigned long)keepintvl) * 1000;
		ret = WSAIoctl((unsigned int)sock, _SIO_KEEPALIVE_VALS, 
			(LPVOID)keepalive, 12, (LPVOID)oldkeep, 12, &retval, NULL, NULL);
		if (ret == SOCKET_ERROR) {
			return -1;
		}
	}	else {
		return -2;
	}
	

#elif defined(SOL_TCL) && defined(TCP_KEEPIDLE) && defined(TCP_KEEPINTVL)
	unsigned long value;
	value = 1;
	isetsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void*)&value, sizeof(long));
	value = keepcnt;
	isetsockopt(sock, SOL_TCP, TCP_KEEPCNT, (void*)value, sizeof(long));
	value = keepidle;
	isetsockopt(sock, SOL_TCP, TCP_KEEPIDLE, (void*)value, sizeof(long));
	value = keepintvl;
	isetsockopt(sock, SOL_TCP, TCP_KEEPINTVL, (void*)value, sizeof(long));
#else
	unsigned long value;
	value = 1;
	isetsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void*)&value, sizeof(long));

	return -1;
#endif

	return 0;
}


/*===================================================================*/
/* ITMCLIENT                                                         */
/*===================================================================*/

void itmc_init(struct ITMCLIENT *client, struct IMEMNODE *nodes, int header)
{
	client->sock = -1;
	client->state = ITMC_STATE_CLOSED;
	client->hid = -1;
	client->tag = -1;
	client->time = 0;
	client->buffer = NULL;
	client->header = (header < 0 || header > 12)? 0 : header;
	ims_init(&client->sendmsg, nodes, 0, 0);
	ims_init(&client->recvmsg, nodes, 0, 0);
}

void itmc_destroy(struct ITMCLIENT *client)
{
	if (client->sock >= 0) iclose(client->sock);
	client->sock = -1;
	client->hid = -1;
	client->tag = -1;
	client->buffer = NULL;
	client->state = ITMC_STATE_CLOSED;
	ims_destroy(&client->sendmsg);
	ims_destroy(&client->recvmsg);
}

int itmc_connect(struct ITMCLIENT *client, const struct sockaddr *addr)
{
	if (client->sock >= 0) iclose(client->sock);
	client->sock = -1;
	client->state = ITMC_STATE_CLOSED;
	client->hid = -1;
	ims_clear(&client->sendmsg);
	ims_clear(&client->recvmsg);
	client->buffer = NULL;
	client->sock = isocket(AF_INET, SOCK_STREAM, 0);
	if (client->sock < 0) return -2;
	ienable(client->sock, ISOCK_NOBLOCK);
	ienable(client->sock, ISOCK_REUSEADDR);
	iconnect(client->sock, addr);
	isockname(client->sock, &client->local);
	client->state = ITMC_STATE_CONNECTING;
	return 0;
}

int itmc_assign(struct ITMCLIENT *client, int sock)
{
	if (client->sock >= 0) iclose(client->sock);
	client->sock = -1;
	client->buffer = NULL;
	ims_clear(&client->sendmsg);
	ims_clear(&client->recvmsg);
	client->sock = sock;
	ienable(client->sock, ISOCK_NOBLOCK);
	ienable(client->sock, ISOCK_REUSEADDR);
	isockname(client->sock, &client->local);
	client->state = ITMC_STATE_ESTABLISHED;
	return 0;
}

int itmc_close(struct ITMCLIENT *client)
{
	if (client->sock >= 0) iclose(client->sock);
	client->sock = -1;
	client->state = ITMC_STATE_CLOSED;
	return 0;
}

static int itmc_try_connect(struct ITMCLIENT *client)
{
	int event;
	if (client->state != 1) return 0;
	event = ISOCK_ESEND | ISOCK_ERROR;
	event = ipollfd(client->sock, event, 0);
	if (event & ISOCK_ERROR) {
		itmc_close(client);
	}	else
	if (event & ISOCK_ESEND) {
		client->state = ITMC_STATE_ESTABLISHED;
	}
	return 0;
}

static void itmc_try_send(struct ITMCLIENT *client)
{
	void *ptr;
	char *flat;
	long size;
	int retval;

	if (client->state != ITMC_STATE_ESTABLISHED) return;

	while (1) {
		size = ims_flat(&client->sendmsg, &ptr);
		if (size <= 0) break;
		flat = (char*)ptr;
		retval = isend(client->sock, flat, size, 0);
		if (retval < 0) {
			retval = ierrno();
			if (retval == EAGAIN) retval = 0;
			else {
				retval = -1;
				itmc_close(client);
				client->error = retval;
				break;
			}
		}
		ims_drop(&client->sendmsg, retval);
	}
}

static void itmc_try_recv(struct ITMCLIENT *client)
{
	unsigned char buffer[0x10000];
	int retval;
	if (client->state != ITMC_STATE_ESTABLISHED) return;
	while (1) {
		retval = irecv(client->sock, buffer, 0x10000, 0);
		if (retval < 0) {
			retval = ierrno();
			if (retval == IEAGAIN) break;
			else { 
				retval = -1;
				itmc_close(client);
				client->error = retval;
				break;
			}
		}	else 
		if (retval == 0) {
			client->error = -1;
			itmc_close(client);
			break;
		}
		ims_write(&client->recvmsg, buffer, retval);
	}
}

int itmc_process(struct ITMCLIENT *client)
{
	if (client->state == ITMC_STATE_CLOSED) {
		return 0;
	}	
	else if (client->state == ITMC_STATE_CONNECTING) {
		itmc_try_connect(client);
	}	
	else {
		itmc_try_send(client);
		itmc_try_recv(client);
	}
	return 0;
}

int itmc_status(struct ITMCLIENT *client)
{
	return client->state;
}

int itmc_nodelay(struct ITMCLIENT *client, int nodelay)
{
	assert(client);
	if (client->sock < 0) return 0;
	if (nodelay) ienable(client->sock, ISOCK_NODELAY);
	else idisable(client->sock, ISOCK_NODELAY);
	return 0;
}

// header size
static const int itmc_head_len[13] = 
	{ 2, 2, 4, 4, 1, 1, 2, 2, 4, 4, 1, 1, 4 };

// header increasement
static const int itmc_head_inc[13] = 
	{ 0, 0, 0, 0, 0, 0, 2, 2, 4, 4, 1, 1, 0 };


// get data size
int itmc_dsize(const struct ITMCLIENT *client)
{
	unsigned char dsize[4];
	unsigned long len;
	IUINT8 len8;
	IUINT16 len16;
	IUINT32 len32;
	int hdrlen;
	int hdrinc;
	int header;

	assert(client);
	hdrlen = itmc_head_len[client->header];
	hdrinc = itmc_head_inc[client->header];

	len = (unsigned short)ims_peek(&client->recvmsg, dsize, hdrlen);
	if (len < hdrlen) return 0;

	if (client->header != ITMH_DWORDMASK) {
		header = (client->header < 6)? client->header : client->header - 6;
	}	else {
		header = ITMH_DWORDLSB;
	}

	switch (header) {
	case ITMH_WORDLSB: 
		idecode16u_lsb((char*)dsize, &len16); 
		len = len16;
		break;
	case ITMH_WORDMSB:
		idecode16u_msb((char*)dsize, &len16); 
		len = len16;
		break;
	case ITMH_DWORDLSB:
		idecode32u_lsb((char*)dsize, &len32);
		if (client->header == ITMH_DWORDMASK) len32 &= 0xffffff;
		len = len32;
		break;
	case ITMH_DWORDMSB:
		idecode32u_msb((char*)dsize, &len32);
		len = len32;
		break;
	case ITMH_BYTELSB:
		idecode8u((char*)dsize, &len8);
		len = len8;
		break;
	case ITMH_BYTEMSB:
		idecode8u((char*)dsize, &len8);
		len = len8;
		break;
	}

	len += hdrinc;

	if (ims_dsize(&client->recvmsg) < len) return 0;
	return (int)len;
}

int itmc_send(struct ITMCLIENT *client, const void *ptr, int size, int mask)
{
	unsigned char dsize[4];
	int header;
	int hdrlen;
	int hdrinc;
	IUINT32 len;

	assert(client);

	hdrlen = itmc_head_len[client->header];
	hdrinc = itmc_head_inc[client->header];

	if (client->header != ITMH_DWORDMASK) {
		header = (client->header < 6)? client->header : client->header - 6;
		len = (IUINT32)size + hdrlen - hdrinc;
		switch (header) {
		case ITMH_WORDLSB:
			iencode16u_lsb((char*)dsize, (IUINT16)len);
			break;
		case ITMH_WORDMSB:
			iencode16u_msb((char*)dsize, (IUINT16)len);
			break;
		case ITMH_DWORDLSB:
			iencode32u_lsb((char*)dsize, (IUINT32)len);
			break;
		case ITMH_DWORDMSB:
			iencode32u_msb((char*)dsize, (IUINT32)len);
			break;
		case ITMH_BYTELSB:
			iencode8u((char*)dsize, (IUINT8)len);
			break;
		case ITMH_BYTEMSB:
			iencode8u((char*)dsize, (IUINT8)len);
			break;
		}
	}	else {
		len = (IUINT32)size + hdrlen - hdrinc;
		len = (len & 0xffffff) | ((((IUINT32)mask) & 0xff) << 24);
		iencode32u_lsb((char*)dsize, (IUINT32)len);
	}

	ims_write(&client->sendmsg, dsize, hdrlen);
	ims_write(&client->sendmsg, ptr, size);

	return 0;
}


int itmc_recv(struct ITMCLIENT *client, void *ptr, int size)
{
	int hdrlen, len;

	assert(client);

	hdrlen = itmc_head_len[client->header];

	len = itmc_dsize(client);
	if (len <= 0) return 0;

	ims_drop(&client->recvmsg, hdrlen);
	len -= hdrlen;

	if (len > size) {
		ims_read(&client->recvmsg, ptr, size);
		ims_drop(&client->recvmsg, len - size);
		return size;
	}

	ims_read(&client->recvmsg, ptr, len);

	return len;
}


int itmc_vsend(struct ITMCLIENT *client, const void *vecptr[], 
	int veclen[], int count, int mask)
{
	unsigned char dsize[4];
	IUINT32 len;
	int hdrlen;
	int hdrinc;
	int header;
	int size;
	int i;

	assert(client);

	for (size = 0, i = 0; i < count; i++) 
		size += veclen[i];

	hdrlen = itmc_head_len[client->header];
	hdrinc = itmc_head_inc[client->header];

	if (client->header != ITMH_DWORDMASK) {
		header = (client->header < 6)? client->header : client->header - 6;
		len = (IUINT32)size + hdrlen - hdrinc;

		switch (header) {
		case ITMH_WORDLSB:
			iencode16u_lsb((char*)dsize, (IUINT16)len);
			break;
		case ITMH_WORDMSB:
			iencode16u_msb((char*)dsize, (IUINT16)len);
			break;
		case ITMH_DWORDLSB:
			iencode32u_lsb((char*)dsize, (IUINT32)len);
			break;
		case ITMH_DWORDMSB:
			iencode32u_msb((char*)dsize, (IUINT32)len);
			break;
		case ITMH_BYTELSB:
			iencode8u((char*)dsize, (IUINT8)len);
			break;
		case ITMH_BYTEMSB:
			iencode8u((char*)dsize, (IUINT8)len);
			break;
		}
	}	else {
		len = (IUINT32)size + hdrlen - hdrinc;
		len = (len & 0xffffff) | ((((IUINT32)mask) & 0xff) << 24);
		iencode32u_lsb((char*)dsize, (IUINT32)len);
	}

	ims_write(&client->sendmsg, dsize, hdrlen);

	for (i = 0; i < count; i++) {
		ims_write(&client->sendmsg, vecptr[i], veclen[i]);
	}

	return 0;
}

int itmc_wait(struct ITMCLIENT *client, int millisec)
{
	IUINT32 clock = 0;
	int retval;
	if (client == NULL) return 0;
	if (client->sock < 0) return 0;

	while (1) {
		itmc_process(client);
		if (itmc_dsize(client) > 0) return ISOCK_ERECV;
		if (client->state == ITMC_STATE_CLOSED) return ISOCK_ERROR;

		if (millisec > 0) {
			if (millisec <= clock) return 0;
			millisec -= (int)clock;
		}

		clock = iclock();
		retval = ipollfd(client->sock, ISOCK_ERECV | ISOCK_ERROR, millisec);
		clock = iclock() - clock;
	}

	return 0;
}


/*===================================================================*/
/* ITMHOST                                                           */
/*===================================================================*/
void itms_init(struct ITMHOST *host, struct IMEMNODE *cache, int header)
{
	assert(host);
	host->needfree = 0;
	host->cache = cache;
	host->nodes = imnode_create(sizeof(struct ITMCLIENT), 64);
	if (host->cache == NULL) {
		host->cache = imnode_create(2048, 64);
		host->needfree = 1;
	}
	assert(host->nodes && host->cache);
	host->buffer = (char*)ikmem_malloc(0x10000);
	assert(host->buffer);
	host->index = 1;
	host->limit = 64 * 1024 * 1024;
	host->port = -1;
	host->sock = -1;
	host->state = 0;
	host->count = 0;
	host->header = header;
	host->timeout = 60000;
	ims_init(&host->event, host->cache, 0, 0);
}

void itms_destroy(struct ITMHOST *host)
{
	assert(host);
	itms_shutdown(host);
	ims_destroy(&host->event);
	if (host->buffer) ikmem_free(host->buffer);
	host->buffer = NULL;
	if (host->nodes) imnode_delete(host->nodes);
	host->nodes = NULL;
	if (host->needfree) {
		imnode_delete(host->cache);
		host->needfree = 0;
	}
	host->cache = NULL;
	host->sock = -1;
	host->state = 0;
	host->count = 0;
	host->timeout = 60000;
}

int itms_startup(struct ITMHOST *host, int port)
{
	struct sockaddr local;
	assert(host);
	itms_shutdown(host);
	ims_clear(&host->event);
	host->sock = (int)socket(AF_INET, SOCK_STREAM, 0);
	if (host->sock < 0) return -1;
	memset(&local, 0, sizeof(local));
	ienable(host->sock, ISOCK_REUSEADDR);
	isockaddr_makeup(&local, "0.0.0.0", port);
	if (ibind(host->sock, &local) != 0) {
		iclose(host->sock);
		host->sock = -1;
		return -2;
	}
	ienable(host->sock, ISOCK_NOBLOCK);
	if (ilisten(host->sock, 10000) != 0) {
		iclose(host->sock);
		host->sock = -1;
		return -3;
	}
	isockname(host->sock, &local);
	host->port = isockaddr_get_port(&local);
	host->index = 1;
	host->count = 0;
	host->state = 1;
	return 0;
}

static inline struct ITMCLIENT *itms_client(struct ITMHOST *host, long hid)
{
	struct ITMCLIENT *client;
	long index = hid & 0xffff;
	if (index < 0 || index >= host->nodes->node_max) return NULL;
	if (IMNODE_MODE(host->nodes, index) == 0) return NULL;
	client = (struct ITMCLIENT*)IMNODE_DATA(host->nodes, index);
	if (client->hid != hid) return NULL;
	return client;
}

static inline void itms_push(struct ITMHOST *host, int event, long wparam,
	long lparam, const void *data, long size)
{
	unsigned char head[12];
	size = size < 0 ? 0 : size;
	*(unsigned short*)(head + 0) = (unsigned short)(size + 12);
	*(unsigned short*)(head + 2) = (unsigned short)event;
	*(long*)(head + 4) = wparam;
	*(long*)(head + 8) = lparam;
	ims_write(&host->event, head, 12);
	ims_write(&host->event, data, size);
}

int itms_shutdown(struct ITMHOST *host)
{
	long hid;
	assert(host);
	for (; ; ) {
		hid = itms_head(host);
		if (hid < 0) break;
		itms_close(host, hid, 0);
	}
	ims_clear(&host->event);
	if (host->sock >= 0) iclose(host->sock);
	host->sock = -1;
	host->state = 0;
	host->count = 0;
	host->index = 1;
	return 0;
}

void itms_process(struct ITMHOST *host)
{
	long index, next, size, delta;
	unsigned long current;
	struct ITMCLIENT *client;
	struct sockaddr remote;
	int sock, error;

	assert(host);
	if (host->state != 1) return;

	current = iclock();

	for (; ; ) {
		sock = iaccept(host->sock, &remote);
		if (sock < 0) break;
		if (host->count >= 0x10000) {
			iclose(sock);
			continue;
		}
		index = imnode_new(host->nodes);
		if (index < 0) {
			iclose(sock);
			continue;
		}
		client = (struct ITMCLIENT*)IMNODE_DATA(host->nodes, index);
		itmc_init(client, host->cache, host->header);
		itmc_assign(client, sock);
		client->hid = (host->index << 16) | index;
		host->index++;
		if (host->index >= 0x7fff) host->index = 1;
		client->tag = -1;
		client->time = current;
		host->count++;
		itms_push(host, ITME_NEW, client->hid, -1, &remote, sizeof(remote));
	}

	for (index = imnode_head(host->nodes); index >= 0; ) {
		next = imnode_next(host->nodes, index);
		client = (struct ITMCLIENT*)IMNODE_DATA(host->nodes, index);

		if (client->state == 2) {
			itmc_try_send(client);
			if (host->event.size <= (IUINT32)host->limit) {
				itmc_try_recv(client);
				for (; ; ) {
					size = itmc_recv(client, host->buffer, 0x10000);
					if (size <= 0) break;
					itms_push(host, ITME_DATA, client->hid, client->tag,
						host->buffer, size);
					client->time = current;
				}
			}
		}

		delta = (long)(current - client->time);
		if (client->state != 2 || delta >= host->timeout) {
			error = client->error;
			if (error == -1) error = 0;
			itms_close(host, client->hid, error);
		}
		
		index = next;
	}
}

void itms_send(struct ITMHOST *host, long hid, const void *data, long size)
{
	struct ITMCLIENT *client;
	assert(host);
	client = itms_client(host, hid);
	if (client == NULL) return;
	itmc_send(client, data, size, 0);
}

void itms_close(struct ITMHOST *host, long hid, int reason)
{
	struct ITMCLIENT *client;
	client = itms_client(host, hid);
	if (client == NULL) return;
	itms_push(host, ITME_LEAVE, client->hid, client->tag, &reason, 4);
	if (client->sock >= 0) iclose(client->sock);
	client->sock = -1;
	client->hid = -1;
	client->tag = -1;
	client->buffer = NULL;
	ims_destroy(&client->sendmsg);
	ims_destroy(&client->recvmsg);
	imnode_del(host->nodes, (hid & 0xffff));
	host->count--;
}

void itms_settag(struct ITMHOST *host, long hid, long tag)
{
	struct ITMCLIENT *client;
	client = itms_client(host, hid);
	if (client == NULL) return;
	client->tag = tag;
}

long itms_gettag(struct ITMHOST *host, long hid)
{
	struct ITMCLIENT *client;
	client = itms_client(host, hid);
	if (client == NULL) return -1;
	return client->tag;
}

void itms_nodelay(struct ITMHOST *host, long hid, int nodelay)
{
	struct ITMCLIENT *client;
	assert(host);
	client = itms_client(host, hid);
	if (client == NULL) return;
	itmc_nodelay(client, nodelay);
}

long itms_read(struct ITMHOST *host, int *msg, long *wparam, long *lparam,
	void *data, long size)
{
	unsigned char head[12];
	struct ITMCLIENT *client;
	long length, canread, id;
	long WPARAM, LPARAM;
	int EVENT;

	assert(host);
	length = ims_peek(&host->event, head, 12);
	if (msg) *msg = -1;
	if (length != 12) return -1;
	length = *(unsigned short*)(head + 0);

	assert(host->event.size >= (IUINT32)length && length >= 12);

	ims_drop(&host->event, 12);
	length -= 12;
	canread = length < size ? length : size;
	if (data) ims_read(&host->event, data, canread);
	else ims_drop(&host->event, canread);
	if (canread < length) ims_drop(&host->event, length - canread);

	EVENT = *(unsigned short*)(head + 2);
	WPARAM = *(long*)(head + 4);
	LPARAM = *(long*)(head + 8);

	if (EVENT == ITME_DATA || EVENT == ITME_LEAVE) {
		id = WPARAM & 0xffff;
		if (id >= 0 && id < host->nodes->node_max) {
			client = (struct ITMCLIENT*)IMNODE_DATA(host->nodes, id);
			if (client->hid == WPARAM) LPARAM = client->tag;
		}
	}

	if (msg) *msg = EVENT;
	if (wparam) *wparam = WPARAM;
	if (lparam) *lparam = LPARAM;

	return canread;
}

long itms_head(struct ITMHOST *host)
{
	struct ITMCLIENT *client;
	long index;
	index = imnode_head(host->nodes);
	if (index < 0) return -1;
	client = (struct ITMCLIENT*)IMNODE_DATA(host->nodes, index);
	return client->hid;
}

long itms_next(struct ITMHOST *host, long hid)
{
	struct ITMCLIENT *client;
	long index;
	index = hid & 0xffff;
	if (index < 0 || index >= host->nodes->node_max) return -1;
	client = (struct ITMCLIENT*)IMNODE_DATA(host->nodes, index);
	if (client->hid != hid) return -1;
	index = IMNODE_NEXT(host->nodes, index);
	if (index < 0) return -1;
	client = (struct ITMCLIENT*)IMNODE_DATA(host->nodes, index);
	return client->hid;
}



/*===================================================================*/
/* IHOSTUDP                                                          */
/*===================================================================*/

void ihu_init(struct IHOSTUDP *uhost, struct IMEMNODE *cache)
{
	assert(uhost);
	uhost->sock = -1;
	uhost->port = -1;
	uhost->state = -1;
	uhost->needsend = 0;
	uhost->needrecv = 0;
	uhost->MTU = 1450;

	uhost->buffer = (char*)ikmem_malloc(0x11000);
	assert(uhost->buffer);

	uhost->cache = cache;
	ims_init(&uhost->recvbuf, cache, 0, 0);
	ims_init(&uhost->sendbuf, cache, 0, 0);

	IMUTEX_INIT(&uhost->lock);
}


void ihu_destroy(struct IHOSTUDP *uhost)
{
	assert(uhost);

	ihu_shutdown(uhost);

	if (uhost->buffer) ikmem_free(uhost->buffer);
	uhost->buffer = NULL;

	ims_destroy(&uhost->recvbuf);
	ims_destroy(&uhost->sendbuf);

	IMUTEX_DESTROY(&uhost->lock);
}

int ihu_startup(struct IHOSTUDP *uhost, int port)
{
	long size1 = (1 << 23);
	long size2 = (1 << 23);
	long size = sizeof(long);

	assert(uhost);
	ihu_shutdown(uhost);

	uhost->sock = inet_open_port((unsigned short)port, 0, 1);
	if (uhost->sock < 0) return -1;

	isockaddr_set(&uhost->local, 0, port);

	isetsockopt(uhost->sock, SOL_SOCKET, SO_RCVBUF, (char*)&size1, size);
	isetsockopt(uhost->sock, SOL_SOCKET, SO_SNDBUF, (char*)&size2, size);

	return 0;
}

int ihu_shutdown(struct IHOSTUDP *uhost)
{
	assert(uhost);
	if (uhost->sock >= 0) {
		iclose(uhost->sock);
		uhost->sock = -1;
	}
	ims_clear(&uhost->recvbuf);
	ims_clear(&uhost->sendbuf);
	uhost->needsend = 0;
	uhost->needrecv = 0;
	memset(&uhost->local, 0, sizeof(struct sockaddr));
	return 0;
}

void ihu_process(struct IHOSTUDP *uhost)
{
	struct sockaddr remote;
	unsigned short size;
	char *buffer, *ptr;
	int retval;

	assert(uhost);
	if (uhost->sock < 0) return;

	buffer = (char*)uhost->buffer;

	for (; ; ) {
		retval = irecvfrom(uhost->sock, buffer, 0x10000, 0, &remote);
		if (retval <= 0) break;
		size = (unsigned short)(retval + sizeof(struct sockaddr) + 2);
		ims_write(&uhost->recvbuf, &size, 2);
		ims_write(&uhost->recvbuf, &remote, sizeof(struct sockaddr));
		ims_write(&uhost->recvbuf, buffer, retval);
		uhost->needrecv++;
	}

	for (; uhost->sendbuf.size > 0; ) {
		retval = ims_peek(&uhost->sendbuf, &size, 2);
		if (retval < 2) break;
		ims_peek(&uhost->sendbuf, buffer, size);
		remote = *(struct sockaddr*)(buffer + 2);
		ptr = buffer + 2 + sizeof(struct sockaddr);
		retval = size - (2 + sizeof(struct sockaddr));
		retval = isendto(uhost->sock, ptr, retval, 0, &remote);
		if (retval <= 0) {
			retval = ierrno();
			if (retval == IEAGAIN) break;
		}
		ims_drop(&uhost->sendbuf, size);
		uhost->needsend--;
	}
}

int ihu_sendto(struct IHOSTUDP *uhost, struct sockaddr *remote, 
	const void *buf, int size)
{
	unsigned short length;
	length = size + (unsigned short)sizeof(struct sockaddr) + 2;
	ims_write(&uhost->sendbuf, &length, 2);
	ims_write(&uhost->sendbuf, remote, sizeof(struct sockaddr));
	ims_write(&uhost->sendbuf, buf, size);
	uhost->needsend++;
	return 0;
}

int ihu_recvfrom(struct IHOSTUDP *uhost, struct sockaddr *remote,
	void *buf, int size)
{
	unsigned short length, smin;
	if (uhost->recvbuf.size < 2) return 0;
	ims_read(&uhost->recvbuf, &length, 2);
	length = length - 2 - sizeof(struct sockaddr);
	ims_read(&uhost->recvbuf, remote, sizeof(struct sockaddr));
	smin = length < size ? length : size;
	ims_read(&uhost->recvbuf, buf, smin);
	if (smin < length) ims_drop(&uhost->recvbuf, length - smin);
	return smin;
}





