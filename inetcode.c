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

#ifdef INADDR_LOOPBACK
	addr1.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#else
	addr1.sin_addr.s_addr = htonl(0x7f000001);
#endif

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



//---------------------------------------------------------------------
// System Utilities
//---------------------------------------------------------------------
#ifndef IDISABLE_SHARED_LIBRARY
	#if defined(__unix)
		#include <dlfcn.h>
	#endif
#endif

void *iutils_shared_open(const char *dllname)
{
#ifndef IDISABLE_SHARED_LIBRARY
	#ifdef __unix
	return dlopen(dllname, RTLD_LAZY);
	#else
	return (void*)LoadLibraryA(dllname);
	#endif
#else
	return NULL;
#endif
}

void *iutils_shared_get(void *shared, const char *name)
{
#ifndef IDISABLE_SHARED_LIBRARY
	#ifdef __unix
	return dlsym(shared, name);
	#else
	return (void*)GetProcAddress((HINSTANCE)shared, name);
	#endif
#else
	return NULL;
#endif
}

void iutils_shared_close(void *shared)
{
#ifndef IDISABLE_SHARED_LIBRARY
	#ifdef __unix
	dlclose(shared);
	#else
	FreeLibrary((HINSTANCE)shared);
	#endif
#endif
}


// load file content
void *iutils_file_load_content(const char *filename, ilong *size)
{
#ifndef IDISABLE_FILE_SYSTEM_ACCESS
	struct IMSTREAM ims;
	size_t length;
	char *ptr;
	FILE *fp;

	ims_init(&ims, NULL, 0, 0);
	fp = fopen(filename, "rb");
	
	ptr = (char*)ikmem_malloc(1024);
	if (ptr == NULL) {
		fclose(fp);
		if (size) size[0] = 0;
		return NULL;
	}
	for (length = 0; ; ) {
		size_t ret = fread(ptr, 1, 1024, fp);
		if (ret == 0) break;
		length += ret;
		ims_write(&ims, ptr, (ilong)ret);
	}

	ikmem_free(ptr);
	fclose(fp);
	
	ptr = (char*)ikmem_malloc((size_t)length);

	if (ptr) {
		ims_read(&ims, ptr, length);
	}	else {
		length = 0;
	}

	ims_destroy(&ims);

	if (size) size[0] = length;

	return ptr;
#else
	return NULL;
#endif
}


// load file content
int iutils_file_load_to_str(const char *filename, ivalue_t *str)
{
	char *ptr;
	ilong size;
	ptr = (char*)iutils_file_load_content(filename, &size);
	if (ptr == NULL) {
		it_sresize(str, 0);
		return -1;
	}
	it_strcpyc(str, ptr, size);
	ikmem_free(ptr);
	return 0;
}

#ifndef IUTILS_STACK_BUFFER_SIZE
#define IUTILS_STACK_BUFFER_SIZE	1024
#endif

#ifndef IDISABLE_FILE_SYSTEM_ACCESS

// load line: returns -1 for end of file, 0 for success
int iutils_file_read_line(FILE *fp, ivalue_t *str)
{
	const int bufsize = IUTILS_STACK_BUFFER_SIZE;
	int size, eof = 0;
	char buffer[IUTILS_STACK_BUFFER_SIZE];

	it_sresize(str, 0);
	for (size = 0, eof = 0; ; ) {
		int ch = fgetc(fp);
		if (ch < 0) {
			eof = 1;
			break;
		}
		buffer[size++] = (unsigned char)ch;
		if (size >= bufsize) {
			it_strcatc(str, buffer, size);
			size = 0;
		}
		if (ch == '\n') break;
	}
	if (size > 0) {
		it_strcatc(str, buffer, size);
	}
	if (eof && it_size(str) == 0) return -1;
	it_strstripc(str, "\r\n");
	return 0;
}

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif


// cross os GetModuleFileName, returns size for success, -1 for error
int iutils_get_proc_pathname(char *ptr, int size)
{
	int retval = -1;
#if defined(_WIN32)
	DWORD hr = GetModuleFileNameA(NULL, ptr, (DWORD)size);
	if (hr > 0) retval = (int)hr;
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
	int mib[4];
	size_t cb = (size_t)size;
	int hr;
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PATHNAME;
	mib[3] = -1;
	hr = sysctl(mib, 4, ptr, &cb, NULL, 0);
	if (hr >= 0) retval = (int)cb;
#elif defined(linux) || defined(__CYGWIN__)
	ilong length;
	char *text;
	text = (char*)iutils_file_load_content("/proc/self/exename", &length);
	if (text) {
		retval = (int)(length < size? length : size);
		memcpy(ptr, text, retval);
		ikmem_free(text);
	}	else {
	}
#else
#endif
	if (retval >= 0 && retval + 1 < size) {
		ptr[retval] = '\0';
	}	else if (size > 0) {
		ptr[0] = '\0';
	}

	if (size > 0) ptr[size - 1] = 0;

	return retval;
}

#endif



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
	client->rc4_send_x = -1;
	client->rc4_send_y = -1;
	client->rc4_recv_x = -1;
	client->rc4_recv_y = -1;
	client->rc4_send_box = NULL;
	client->rc4_recv_box = NULL;
	ims_init(&client->sendmsg, nodes, 0, 0);
	ims_init(&client->recvmsg, nodes, 0, 0);
}

void itmc_destroy(struct ITMCLIENT *client)
{
	if (client->sock >= 0) iclose(client->sock);
	if (client->buffer) ikmem_free(client->buffer);
	client->buffer = NULL;
	client->sock = -1;
	client->hid = -1;
	client->tag = -1;
	client->buffer = NULL;
	client->state = ITMC_STATE_CLOSED;
	ims_destroy(&client->sendmsg);
	ims_destroy(&client->recvmsg);
	client->rc4_send_box = NULL;
	client->rc4_recv_box = NULL;
	client->rc4_send_x = -1;
	client->rc4_send_y = -1;
	client->rc4_recv_x = -1;
	client->rc4_recv_y = -1;
}

#ifndef ITMC_BUFSIZE
#define ITMC_BUFSIZE	0x10000
#endif

int itmc_connect(struct ITMCLIENT *client, const struct sockaddr *addr)
{
	if (client->sock >= 0) iclose(client->sock);

	client->sock = -1;
	client->state = ITMC_STATE_CLOSED;
	client->hid = -1;

	ims_clear(&client->sendmsg);
	ims_clear(&client->recvmsg);

	if (client->buffer == NULL) {
		client->buffer = (char*)ikmem_malloc(ITMC_BUFSIZE + 512);
		if (client->buffer == NULL) return -1;
		client->rc4_send_box = (unsigned char*)client->buffer + ITMC_BUFSIZE;
		client->rc4_recv_box = client->rc4_send_box + 256;
	}

	client->rc4_send_x = -1;
	client->rc4_send_y = -1;
	client->rc4_recv_x = -1;
	client->rc4_recv_y = -1;

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

	if (client->buffer == NULL) {
		client->buffer = (char*)ikmem_malloc(ITMC_BUFSIZE + 512);
		if (client->buffer == NULL) return -1;
		client->rc4_send_box = (unsigned char*)client->buffer + ITMC_BUFSIZE;
		client->rc4_recv_box = client->rc4_send_box + 256;
	}

	client->rc4_send_x = -1;
	client->rc4_send_y = -1;
	client->rc4_recv_x = -1;
	client->rc4_recv_y = -1;

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
	client->rc4_send_x = -1;
	client->rc4_send_y = -1;
	client->rc4_recv_x = -1;
	client->rc4_recv_y = -1;
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
			if (retval == IEAGAIN) retval = 0;
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
	unsigned char *buffer = (unsigned char*)client->buffer;
	int retval;
	if (client->state != ITMC_STATE_ESTABLISHED) return;
	while (1) {
		retval = irecv(client->sock, buffer, ITMC_BUFSIZE, 0);
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
		if (client->rc4_recv_x >= 0 && client->rc4_recv_y >= 0) {
			icrypt_rc4_crypt(client->rc4_recv_box, &client->rc4_recv_x,
				&client->rc4_recv_y, buffer, buffer, retval);
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
	if (len < (unsigned long)hdrlen) return 0;

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

	if (ims_dsize(&client->recvmsg) < (ilong)len) return 0;
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

	if (client->rc4_send_x < 0 || client->rc4_send_y < 0) {
		ims_write(&client->sendmsg, dsize, hdrlen);
		ims_write(&client->sendmsg, ptr, size);
	}	else {
		unsigned char *buffer = (unsigned char*)client->buffer;
		const unsigned char *lptr = (const unsigned char*)ptr;
		long length = size;
		icrypt_rc4_crypt(client->rc4_send_box, &client->rc4_send_x, 
			&client->rc4_send_y, dsize, dsize, hdrlen);
		ims_write(&client->sendmsg, dsize, hdrlen);
		for (; length > 0; ) {
			long canread = (length > ITMC_BUFSIZE)? ITMC_BUFSIZE : length;
			icrypt_rc4_crypt(client->rc4_send_box, &client->rc4_send_x, 
				&client->rc4_send_y, lptr, buffer, canread);
			ims_write(&client->sendmsg, buffer, canread);
			length -= canread;
			lptr += canread;
		}
	}

	return 0;
}


int itmc_recv(struct ITMCLIENT *client, void *ptr, int size)
{
	int hdrlen, len;

	assert(client);

	hdrlen = itmc_head_len[client->header];

	len = itmc_dsize(client);
	if (len <= 0) return -1;

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

	if (client->rc4_send_x >= 0 && client->rc4_send_y >= 0) {
		icrypt_rc4_crypt(client->rc4_send_box, &client->rc4_send_x,
			&client->rc4_send_y, dsize, dsize, hdrlen);
	}

	ims_write(&client->sendmsg, dsize, hdrlen);

	for (i = 0; i < count; i++) {
		if (client->rc4_send_x < 0 || client->rc4_send_y < 0) {
			ims_write(&client->sendmsg, vecptr[i], veclen[i]);
		}	else {
			unsigned char *buffer = (unsigned char*)client->buffer;
			const unsigned char *lptr = (const unsigned char*)vecptr[i];
			long size = veclen[i];
			for (; size > 0; ) {
				long canread = (size > ITMC_BUFSIZE)? ITMC_BUFSIZE : size;
				icrypt_rc4_crypt(client->rc4_send_box, &client->rc4_send_x, 
					&client->rc4_send_y, lptr, buffer, canread);
				ims_write(&client->sendmsg, buffer, canread);
				size -= canread;
				lptr += canread;
			}
		}
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
			if (millisec <= (int)clock) return 0;
			millisec -= (int)clock;
		}

		clock = iclock();
		retval = ipollfd(client->sock, ISOCK_ERECV | ISOCK_ERROR, millisec);
		clock = iclock() - clock;
	}

	return 0;
}

void itmc_rc4_set_skey(struct ITMCLIENT *client, 
	const unsigned char *key, int keylen)
{
	if (client->rc4_send_box) {
		icrypt_rc4_init(client->rc4_send_box, &client->rc4_send_x,
			&client->rc4_send_y, key, keylen);
	}
}

void itmc_rc4_set_rkey(struct ITMCLIENT *client, 
	const unsigned char *key, int keylen)
{
	if (client->rc4_recv_box) {
		icrypt_rc4_init(client->rc4_recv_box, &client->rc4_recv_x,
			&client->rc4_recv_y, key, keylen);
	}
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
	char head[14];
	size = size < 0 ? 0 : size;
	iencode32u_lsb(head, (long)(size + 14));
	iencode16u_lsb(head + 4, (unsigned short)event);
	iencode32u_lsb(head + 6, wparam);
	iencode32u_lsb(head + 10, lparam);
	ims_write(&host->event, head, 14);
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
	char head[14];
	struct ITMCLIENT *client;
	long length, canread, id;
	long WPARAM, LPARAM;
	IUINT32 x; IUINT16 y;
	int EVENT;

	assert(host);
	length = ims_peek(&host->event, head, 14);
	if (msg) *msg = -1;
	if (length != 14) return -1;
	
	idecode32u_lsb(head, &x);
	length = x;

	assert(host->event.size >= (IUINT32)length && length >= 14);

	ims_drop(&host->event, 14);
	length -= 14;
	canread = length < size ? length : size;
	if (data) ims_read(&host->event, data, canread);
	else ims_drop(&host->event, canread);
	if (canread < length) ims_drop(&host->event, length - canread);

	idecode16u_lsb(head + 4, &y);
	EVENT = y;
	idecode32u_lsb(head + 6, &x);
	WPARAM = (long)x;
	idecode32u_lsb(head + 10, &x);
	LPARAM = (long)x;

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



//---------------------------------------------------------------------
// PROXY
//---------------------------------------------------------------------
#define ISOCKPROXY_IN		1
#define ISOCKPROXY_OUT		2
#define ISOCKPROXY_ERR		4

#define ISOCKPROXY_FAILED		(-1)
#define ISOCKPROXY_START		0
#define ISOCKPROXY_CONNECTING	1
#define ISOCKPROXY_SENDING1		2
#define ISOCKPROXY_RECVING1		3
#define ISOCKPROXY_SENDING2		4
#define ISOCKPROXY_RECVING2		5
#define ISOCKPROXY_SENDING3		6
#define ISOCKPROXY_RECVING3		7
#define ISOCKPROXY_CONNECTED	10

///
/// 转换编码格式 iproxy_base64
/// 成功返回长度
///
int iproxy_base64(const unsigned char *in, unsigned char *out, int size)
{
	const char base64[] = 
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	unsigned char fragment; 
	unsigned char*saveout = out;

	for (; size >= 3; size -= 3) {
		*out++ = base64[in[0] >> 2];
		*out++ = base64[((in[0] << 4) & 0x30) | (in[1] >> 4)];
		*out++ = base64[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
		*out++ = base64[in[2] & 0x3f];
		in += 3;
	}
	if (size > 0) {
		*out++ = base64[in[0] >> 2];
		fragment = (in[0] << 4) & 0x30;
		if (size > 1) fragment |= in[1] >> 4;
		*out++ = base64[fragment];
		*out++ = (size < 2) ? '=' : base64[(in[1] << 2) & 0x3c];
		*out++ = '=';
	}
	*out = '\0';
	return saveout - out;
};


///
/// 做一次POLL操作
///
int iproxy_poll(int sock, int event, long millisec)
{
	int retval = 0;

	#if defined(__unix) && (!defined(__llvm__))
	struct pollfd pfd;
	
	pfd.fd = sock;
	pfd.events = 0;
	pfd.revents = 0;

	pfd.events |= (event & ISOCKPROXY_IN)? POLLIN : 0;
	pfd.events |= (event & ISOCKPROXY_OUT)? POLLOUT : 0;
	pfd.events |= (event & ISOCKPROXY_ERR)? POLLERR : 0;

	poll(&pfd, 1, millisec);

	if ((event & ISOCKPROXY_IN) && (pfd.revents & POLLIN)) 
		retval |= ISOCKPROXY_IN;
	if ((event & ISOCKPROXY_OUT) && (pfd.revents & POLLOUT)) 
		retval |= ISOCKPROXY_OUT;
	if ((event & ISOCKPROXY_ERR) && (pfd.revents & POLLERR)) 
		retval |= ISOCKPROXY_ERR;
	#elif defined(__llvm__) 
	struct timeval tmx = { 0, 0 };
	fd_set fdr, fdw, fde;
	fd_set *pr = NULL, *pw = NULL, *pe = NULL;
	tmx.tv_sec = millisec / 1000;
	tmx.tv_usec = (millisec % 1000) * 1000;
	if (event & ISOCKPROXY_IN) {
		FD_ZERO(&fdr);
		FD_SET(sock, &fdr);
		pr = &fdr;
	}
	if (event & ISOCKPROXY_OUT) {
		FD_ZERO(&fdw);
		FD_SET(sock, &fdw);
		pw = &fdw;
	}
	if (event & ISOCKPROXY_ERR) {
		FD_ZERO(&fde);
		FD_SET(sock, &fde);
		pe = &fde;
	}
	retval = select(sock + 1, pr, pw, pe, (millisec >= 0)? &tmx : 0);
	retval = 0;
	if ((event & ISOCKPROXY_IN) && FD_ISSET(sock, &fdr))
		event |= ISOCKPROXY_IN;
	if ((event & ISOCKPROXY_OUT) && FD_ISSET(sock, &fdw)) 
		event |= ISOCKPROXY_OUT;
	if ((event & ISOCKPROXY_ERR) && FD_ISSET(sock, &fde)) 
		event |= ISOCKPROXY_ERR;
	#else
	struct timeval tmx = { 0, 0 };
	union { void *ptr; fd_set *fds; } p[3];
	int fdr[2], fdw[2], fde[2];

	tmx.tv_sec = millisec / 1000;
	tmx.tv_usec = (millisec % 1000) * 1000;
	fdr[0] = fdw[0] = fde[0] = 1;
	fdr[1] = fdw[1] = fde[1] = sock;

	p[0].ptr = (event & ISOCKPROXY_IN)? fdr : NULL;
	p[1].ptr = (event & ISOCKPROXY_OUT)? fdw : NULL;
	p[2].ptr = (event & ISOCKPROXY_ERR)? fde : NULL;

	retval = select( sock + 1, p[0].fds, p[1].fds, p[2].fds, 
					(millisec >= 0)? &tmx : 0);
	retval = 0;

	if ((event & ISOCKPROXY_IN) && fdr[0]) retval |= ISOCKPROXY_IN;
	if ((event & ISOCKPROXY_OUT) && fdw[0]) retval |= ISOCKPROXY_OUT;
	if ((event & ISOCKPROXY_ERR) && fde[0]) retval |= ISOCKPROXY_ERR;
	#endif

	return retval;
}


///
/// 返回错误码
///
int iproxy_errno(void)
{
	int retval;
	#ifdef __unix
	retval = errno;
	#else
	retval = (int)WSAGetLastError();
	#endif
	return retval;
}


///
/// 发送数据
///
int iproxy_send(struct ISOCKPROXY *proxy)
{
	int retval;

	if (proxy->offset >= proxy->totald) return 0;
	if (iproxy_poll(proxy->socket, ISOCKPROXY_OUT | ISOCKPROXY_ERR, 0) == 0)
		return 0;

	retval = send(proxy->socket, proxy->data + proxy->offset, 
		proxy->totald - proxy->offset, 0);
	if (retval == 0) return -1;
	if (retval == -1) {
		if (iproxy_errno() == IEAGAIN) return 0;
		return -2;
	}
	proxy->offset = proxy->offset + retval;

	return retval;
}


///
/// 接收数据
///
int iproxy_recv(struct ISOCKPROXY *proxy, int max)
{
	int retval;
	int msize;

	if (iproxy_poll(proxy->socket, ISOCKPROXY_IN | ISOCKPROXY_ERR, 0) == 0) 
		return 0;

	max = (max <= 0)? 0x10000 : max;
	msize = (proxy->offset < max)? max - proxy->offset : 0;
	if (msize == 0) return 0;

	retval = recv(proxy->socket, proxy->data + proxy->offset, msize, 0);
	if (retval == 0) return -1;
	if (retval == -1) return (iproxy_errno() == IEAGAIN)? 0 : -2;

	proxy->offset = proxy->offset + retval;
	proxy->data[proxy->offset] = 0;
	//printf("[PROXY_DATA] size=%d:\n{%s}\n", retval, proxy->data);

	return retval;
}


///
/// 初始化连接数据 ISOCKPROXY
/// 选择 type有：ISOCKPROXY_TYPE_NONE, ISOCKPROXY_TYPE_HTTP, 
//  ISOCKPROXY_TYPE_SOCKS4, ISOCKPROXY_TYPE_SOCKS5
/// 
int iproxy_init(struct ISOCKPROXY *proxy, int sock, int type, 
	const struct sockaddr *remote, const struct sockaddr *proxyd, 
	const char *user, const char *pass, int mode)
{
	struct sockaddr_in *endpoint = (struct sockaddr_in*)remote;
	unsigned char *bytes = (unsigned char*)&(endpoint->sin_addr.s_addr);
	int authent = (user == NULL)? 0 : 1;
	int ips[5], i, j;
	char auth[512], auth64[512];
	char addr[64];

	proxy->socket = sock;
	proxy->type = type;
	proxy->next = 0;
	proxy->offset = 0;
	proxy->totald = 0;
	proxy->errorc = 0;
	proxy->remote = *remote;
	proxy->proxyd = *proxyd;
	proxy->authen = authent;

	for (i = 0; i < 4; i++) ips[i] = (unsigned char)bytes[i];
	ips[4] = (int)(htons(endpoint->sin_port));
	sprintf(addr, "%d.%d.%d.%d:%d", ips[0], ips[1], ips[2], ips[3], ips[4]);

	switch (proxy->type)
	{
	case ISOCKPROXY_TYPE_HTTP:
		if (authent == 0) {
			sprintf(proxy->data, "CONNECT %s HTTP/1.0\r\n\r\n", addr);
		}	else {
			sprintf(auth, "%s:%s", user, pass);
			iproxy_base64((unsigned char*)auth, (unsigned char*)auth64, 
				strlen(auth));
			sprintf(proxy->data, 
			"CONNECT %s HTTP/1.0\r\nProxy-Authorization: Basic %s\r\n\r\n", 
			addr, auth64);
		}
		proxy->totald = strlen(proxy->data);
		proxy->data[proxy->totald] = 0;
		break;

	case ISOCKPROXY_TYPE_SOCKS4:
		proxy->data[0] = 4;
		proxy->data[1] = 1;
		memcpy(proxy->data + 2, &(endpoint->sin_port), 2);
		memcpy(proxy->data + 4, &(endpoint->sin_addr), 4);
		proxy->data[8] = 0;
		proxy->totald = 0;
		break;

	case ISOCKPROXY_TYPE_SOCKS5:
		if (authent == 0) {
			proxy->data[0] = 5;
			proxy->data[1] = 1;
			proxy->data[2] = 0;
			proxy->totald = 3;
		}	else {
			proxy->data[0] = 5;
			proxy->data[1] = 2;
			proxy->data[2] = 0;
			proxy->data[3] = 2;
			proxy->totald = 4;
		}
		proxy->data[402] = 5;
		proxy->data[403] = 1;
		proxy->data[404] = 0;
		proxy->data[405] = 3;
		sprintf(addr, "%d.%d.%d.%d", ips[0], ips[1], ips[2], ips[3]);
		proxy->data[406] = strlen(addr);
		memcpy(proxy->data + 407, addr, strlen(addr));
		memcpy(proxy->data + 407 + strlen(addr), &(endpoint->sin_port), 2);
		*(short*)(proxy->data + 400) = 7 + strlen(addr);
		if (authent) {
			i = strlen(user);
			j = strlen(pass);
			proxy->data[702] = 1;
			proxy->data[703] = i;
			memcpy(proxy->data + 704, user, i);
			proxy->data[704 + i] = j;
			memcpy(proxy->data + 704 + i + 1, pass, j);
			*(short*)(proxy->data + 700) = 3 + i + j;
		}
		break;
	}

	return 0;
}


///
/// 处理请求
/// 成功返回1，失败返回<0，阻塞返回0
///
int iproxy_process(struct ISOCKPROXY *proxy)
{
	struct sockaddr *remote;
	int retval = 0;

	proxy->block = 0;

	if (proxy->next == ISOCKPROXY_START) {
		remote = (proxy->type == ISOCKPROXY_TYPE_NONE)? 
			&(proxy->remote) : &(proxy->proxyd);
		retval = connect(proxy->socket, remote, sizeof(struct sockaddr));
		if (retval == 0) proxy->next = ISOCKPROXY_CONNECTING;
		else proxy->next = (iproxy_errno() == IEAGAIN)? 
			ISOCKPROXY_CONNECTING : ISOCKPROXY_FAILED;	
		if (proxy->next == ISOCKPROXY_FAILED) proxy->errorc = 1;
	}

	if (proxy->next == ISOCKPROXY_CONNECTING) {
		retval = iproxy_poll(proxy->socket, 
			ISOCKPROXY_OUT | ISOCKPROXY_ERR, 0);
		if (retval & ISOCKPROXY_ERR) {
			proxy->errorc = 2;
			proxy->next = ISOCKPROXY_FAILED;
		}	else
		if (retval & ISOCKPROXY_OUT) {
			if (proxy->type == ISOCKPROXY_TYPE_NONE) 
				proxy->next = ISOCKPROXY_CONNECTED;
			else proxy->next = ISOCKPROXY_SENDING1;
		}
	}

	if (proxy->next == ISOCKPROXY_SENDING1) {
		retval = iproxy_send(proxy);
		if (retval < 0) proxy->next = ISOCKPROXY_FAILED, proxy->errorc = 3;
		else if (proxy->offset >= proxy->totald) {
			proxy->data[proxy->offset] = 0;
			proxy->next = ISOCKPROXY_RECVING1;
			proxy->offset = 0;
		}
	}

	if (proxy->next == ISOCKPROXY_FAILED) return -1;
	if (proxy->next == ISOCKPROXY_CONNECTED) return 1;

	if (proxy->type == ISOCKPROXY_TYPE_NONE) return 0;

	if (proxy->type == ISOCKPROXY_TYPE_HTTP) {
		if (proxy->next == ISOCKPROXY_RECVING1) {
			for (; proxy->next == ISOCKPROXY_RECVING1; ) {
				retval = iproxy_recv(proxy, proxy->offset + 1);
				proxy->data[proxy->offset] = 0;
				if (retval ==0) break;
				if (retval < 0) {
					proxy->next = ISOCKPROXY_FAILED;
					proxy->errorc = 10;
				}	else if (proxy->offset > 4) {
					if (strcmp(proxy->data + proxy->offset - 4, 
						"\r\n\r\n") == 0) {
						retval = 0;
						if (memcmp(proxy->data, "HTTP/1.0 200", 
							strlen("HTTP/1.0 200")) == 0) retval = 1;
						if (memcmp(proxy->data, "HTTP/1.1 200", 
							strlen("HTTP/1.1 200")) == 0) retval = 1;
						if (retval == 1) proxy->next = ISOCKPROXY_CONNECTED;
						else {
							proxy->next = ISOCKPROXY_FAILED;
							proxy->errorc = 11;
						}
					}
				}
			}
		}
	}	else
	if (proxy->type == ISOCKPROXY_TYPE_SOCKS4) {
		if (proxy->next == ISOCKPROXY_RECVING1) {
			if (iproxy_recv(proxy, 8) < 0) {
				proxy->next = ISOCKPROXY_FAILED;
				proxy->errorc = 20;
			}
			else if (proxy->offset >= 8) {
				if (proxy->data[0] == 0 && proxy->data[1] == 90) 
					proxy->next = ISOCKPROXY_CONNECTED;
				else {
					proxy->next = ISOCKPROXY_FAILED;
					proxy->errorc = 21;
				}
			}
		}
	}	else
	if (proxy->type == ISOCKPROXY_TYPE_SOCKS5) {
		if (proxy->next == ISOCKPROXY_RECVING1) {
			retval = iproxy_recv(proxy, -1);
			if (retval < 0) {
				proxy->next = ISOCKPROXY_FAILED;
				proxy->errorc = 31;
			}
			else if (proxy->offset >= 2) {
				if (proxy->authen == 0) {
					if (proxy->data[0] == 5 && proxy->data[1] == 0) {
						memcpy(proxy->data, proxy->data + 402, 
							*(short*)(proxy->data + 400));
						proxy->totald = *(short*)(proxy->data + 400);
						proxy->next = ISOCKPROXY_SENDING3;
					}	else {
						proxy->next = ISOCKPROXY_FAILED; 
						proxy->errorc = 32;
					}
				}	else {
					if (proxy->data[0] == 5 && proxy->data[1] == 0) {
						memcpy(proxy->data, proxy->data + 402,
							*(short*)(proxy->data + 400));
						proxy->totald = *(short*)(proxy->data + 400);
						proxy->next = ISOCKPROXY_SENDING3;
					}	else
					if (proxy->data[0] == 5 && proxy->data[1] == 2) {
						memcpy(proxy->data, proxy->data + 702, 
							*(short*)(proxy->data + 700));
						proxy->totald = *(short*)(proxy->data + 700);
						proxy->next = ISOCKPROXY_SENDING2;
					}	else {
						proxy->next = ISOCKPROXY_FAILED;
						proxy->errorc = 33;
					}
				}
				proxy->offset = 0;
			}
		}
		if (proxy->next == ISOCKPROXY_SENDING2) {
			if (iproxy_send(proxy) < 0) {
				proxy->next = ISOCKPROXY_FAILED;
				proxy->errorc = 40;
			}
			else if (proxy->offset >= proxy->totald) {
				proxy->next = ISOCKPROXY_RECVING2;
				proxy->offset = 0;
			}
		}
		if (proxy->next == ISOCKPROXY_RECVING2) {
			if (iproxy_recv(proxy, -1) < 0) {
				proxy->next = ISOCKPROXY_FAILED;
				proxy->errorc = 41;
			}
			else if (proxy->offset >= 2) {
				if (proxy->data[1] != 0) {
					proxy->next = ISOCKPROXY_FAILED;
					proxy->errorc = 42;
				}
				else {
					memcpy(proxy->data, proxy->data + 402, 
						*(short*)(proxy->data + 400));
					proxy->totald = *(short*)(proxy->data + 400);
					proxy->next = ISOCKPROXY_SENDING3;
					proxy->offset = 0;
				}
			}
		}
		if (proxy->next == ISOCKPROXY_SENDING3) {
			if (iproxy_send(proxy) < 0) {
				proxy->next = ISOCKPROXY_FAILED;
				proxy->errorc = 50;
			}
			else if (proxy->offset >= proxy->totald) {
				proxy->next = ISOCKPROXY_RECVING3;
				proxy->offset = 0;
			}
		}
		if (proxy->next == ISOCKPROXY_RECVING3) {
			retval = iproxy_recv(proxy, 10);
			if (retval < 0) {
				proxy->next = ISOCKPROXY_FAILED;
				proxy->errorc = 51;
			}
			else if (proxy->offset >= 10) {
				if (proxy->data[0] == 5 && proxy->data[1] == 0) 
					proxy->next = ISOCKPROXY_CONNECTED;
				else proxy->next = ISOCKPROXY_FAILED, proxy->errorc = 52;
			}
		}
	}	else {
		proxy->errorc = 100;
		proxy->next = ISOCKPROXY_FAILED;
	}

	if (proxy->next == ISOCKPROXY_FAILED) return -1;
	if (proxy->next == ISOCKPROXY_CONNECTED) return 1;

	return 0;
}




//---------------------------------------------------------------------
// CSV Reader/Writer
//---------------------------------------------------------------------
struct iCsvReader
{
	istring_list_t *source;
	istring_list_t *strings;
#ifndef IDISABLE_FILE_SYSTEM_ACCESS
	FILE *fp;
#endif
	ivalue_t string;
	ilong line;
	int count;
};

struct iCsvWriter
{
	ivalue_t string;
	ivalue_t output;
	int mode;
	istring_list_t *strings;
#ifndef IDISABLE_FILE_SYSTEM_ACCESS
	FILE *fp;
#endif
};


/* open csv reader from file */
iCsvReader *icsv_reader_open_file(const char *filename)
{
#ifndef IDISABLE_FILE_SYSTEM_ACCESS
	iCsvReader *reader;
	FILE *fp;
	fp = fopen(filename, "rb");
	if (fp == NULL) return NULL;
	reader = (iCsvReader*)ikmem_malloc(sizeof(iCsvReader));
	if (reader == NULL) {
		fclose(fp);
		return NULL;
	}
	it_init(&reader->string, ITYPE_STR);
	reader->fp = fp;
	reader->source = NULL;
	reader->strings = NULL;
	reader->line = 0;
	reader->count = 0;
	return reader;
#else
	return NULL;
#endif
}

/* open csv reader from memory */
iCsvReader *icsv_reader_open_memory(const char *text, ilong size)
{
	iCsvReader *reader;
	reader = (iCsvReader*)ikmem_malloc(sizeof(iCsvReader));
	if (reader == NULL) {
		return NULL;
	}

	it_init(&reader->string, ITYPE_STR);
#ifndef IDISABLE_FILE_SYSTEM_ACCESS
	reader->fp = NULL;
#endif
	reader->source = NULL;
	reader->strings = NULL;
	reader->line = 0;
	reader->count = 0;

	reader->source = istring_list_split(text, size, "\n", 1);
	if (reader->source == NULL) {
		ikmem_free(reader);
		return NULL;
	}

	return reader;
}

void icsv_reader_close(iCsvReader *reader)
{
	if (reader) {
		if (reader->strings) {
			istring_list_delete(reader->strings);
			reader->strings = NULL;
		}
		if (reader->source) {
			istring_list_delete(reader->source);
			reader->source = NULL;
		}
#ifndef IDISABLE_FILE_SYSTEM_ACCESS
		if (reader->fp) {
			fclose(reader->fp);
			reader->fp = NULL;
		}
#endif
		reader->line = 0;
		reader->count = 0;
		it_destroy(&reader->string);
		ikmem_free(reader);
	}
}

// parse row
void icsv_reader_parse(iCsvReader *reader, ivalue_t *str)
{
	if (reader->strings) {
		istring_list_delete(reader->strings);
		reader->strings = NULL;
	}

	reader->strings = istring_list_csv_decode(it_str(str), it_size(str));
	reader->count = 0;

	if (reader->strings) {
		reader->count = (int)reader->strings->count;
	}
}

// read csv row
int icsv_reader_read(iCsvReader *reader)
{
	if (reader == NULL) return 0;
	if (reader->strings) {
		istring_list_delete(reader->strings);
		reader->strings = NULL;
	}
	reader->count = 0;
	if (reader->source) {	// 使用文本模式
		ivalue_t *str;
		if (reader->line >= reader->source->count) {
			istring_list_delete(reader->source);
			reader->source = NULL;
			return -1;
		}
		str = reader->source->values[reader->line++];
		it_strstripc(str, "\r\n");
		icsv_reader_parse(reader, str);
	}
#ifndef IDISABLE_FILE_SYSTEM_ACCESS
	else if (reader->fp) {
		if (iutils_file_read_line(reader->fp, &reader->string) != 0) {
			fclose(reader->fp);
			reader->fp = 0;
			return -1;
		}
		reader->line++;
		it_strstripc(&reader->string, "\r\n");
		icsv_reader_parse(reader, &reader->string);
	}
#endif
	else {
		reader->count = 0;
		return -1;
	}
	if (reader->strings == NULL) 
		return -1;
	return reader->count;
}

// get column count in current row
int icsv_reader_size(const iCsvReader *reader)
{
	return reader->count;
}

// returns 1 for end of file, 0 for not end.
int icsv_reader_eof(const iCsvReader *reader)
{
	void *fp = NULL;
#ifndef IDISABLE_FILE_SYSTEM_ACCESS
	fp = (void*)reader->fp;
#endif
	if (fp == NULL && reader->source == NULL) return 1;
	return 0;
}

// get column string
ivalue_t *icsv_reader_get(iCsvReader *reader, int pos)
{
	if (reader == NULL) return NULL;
	if (pos < 0 || pos >= reader->count) return NULL;
	if (reader->strings == NULL) return NULL;
	return reader->strings->values[pos];
}

// get column string
const ivalue_t *icsv_reader_get_const(const iCsvReader *reader, int pos)
{
	if (reader == NULL) return NULL;
	if (pos < 0 || pos >= reader->count) return NULL;
	if (reader->strings == NULL) return NULL;
	return reader->strings->values[pos];
}

// return column string size, -1 for error
int icsv_reader_get_size(const iCsvReader *reader, int pos)
{
	const ivalue_t *str = icsv_reader_get_const(reader, pos);
	if (str == NULL) return -1;
	return (int)it_size(str);
}

// return column string, returns string size for success, -1 for error
int icsv_reader_get_string(const iCsvReader *reader, int pos, ivalue_t *out)
{
	const ivalue_t *str = icsv_reader_get_const(reader, pos);
	if (str == NULL) {
		it_sresize(out, 0);
		return -1;
	}
	it_cpy(out, str);
	return (int)it_size(str);
}

// return column string, returns string size for success, -1 for error
int icsv_reader_get_cstr(const iCsvReader *reader, int pos, 
	char *out, int size)
{
	const ivalue_t *str = icsv_reader_get_const(reader, pos);
	if (str == NULL) {
		if (size > 0) out[0] = 0;
		return -1;
	}
	if ((ilong)it_size(str) > (ilong)size) {
		if (size > 0) out[0] = 0;
		return -1;
	}
	memcpy(out, it_str(str), it_size(str));
	if (size > (int)it_size(str) + 1) {
		out[it_size(str)] = 0;
	}
	return (int)it_size(str);
}



// open csv writer from file: if filename is NULL, it will open in memory
iCsvWriter *icsv_writer_open(const char *filename, int append)
{
	iCsvWriter *writer;

	writer = (iCsvWriter*)ikmem_malloc(sizeof(iCsvWriter));
	if (writer == NULL) return NULL;

	if (filename != NULL) {
		void *fp = NULL;
#ifndef IDISABLE_FILE_SYSTEM_ACCESS
		writer->fp = fopen(filename, append? "a" : "w");
		if (writer->fp && append) {
			fseek(writer->fp, 0, SEEK_END);
		}
		fp = writer->fp;
#endif
		if (fp == NULL) {
			ikmem_free(writer);
			return NULL;
		}
		writer->mode = 1;
	}	else {
		writer->mode = 2;
#ifndef IDISABLE_FILE_SYSTEM_ACCESS
		writer->fp = NULL;
#endif
	}

	writer->strings = istring_list_new();

	if (writer->strings == NULL) {
#ifndef IDISABLE_FILE_SYSTEM_ACCESS
		if (writer->fp) {
			fclose(writer->fp);
		}
#endif
		ikmem_free(writer);
		return NULL;
	}

	it_init(&writer->string, ITYPE_STR);
	it_init(&writer->output, ITYPE_STR);

	return writer;
}

// close csv writer
void icsv_writer_close(iCsvWriter *writer)
{
	if (writer) {
		if (writer->strings) {
			istring_list_delete(writer->strings);
			writer->strings = NULL;
		}
#ifndef IDISABLE_FILE_SYSTEM_ACCESS
		if (writer->fp) {
			fclose(writer->fp);
			writer->fp = NULL;
		}
#endif
		writer->mode = 0;
		it_destroy(&writer->string);
		it_destroy(&writer->output);
		ikmem_free(writer);
	}
}

// write row and reset
int icsv_writer_write(iCsvWriter *writer)
{
	istring_list_csv_encode(writer->strings, &writer->string);
	it_strcatc(&writer->string, "\n", 1);
	if (writer->mode == 1) {
#ifndef IDISABLE_FILE_SYSTEM_ACCESS
		if (writer->fp) {
			fwrite(it_str(&writer->string), 1, 
				it_size(&writer->string), writer->fp);
}
#endif
	}	
	else if (writer->mode == 2) {
		it_strcat(&writer->output, &writer->string);
	}
	istring_list_clear(writer->strings);
	return 0;
}

// return column count in current row
int icsv_writer_size(iCsvWriter *writer)
{
	return writer->strings->count;
}

// clear columns in current row
void icsv_writer_clear(iCsvWriter *writer)
{
	istring_list_clear(writer->strings);
}

// dump output
void icsv_writer_dump(iCsvWriter *writer, ivalue_t *out)
{
	it_cpy(out, &writer->output);
}

// clear output
void icsv_writer_empty(iCsvWriter *writer)
{
	it_sresize(&writer->output, 0);
}

// push string
int icsv_writer_push(iCsvWriter *writer, const ivalue_t *str)
{
	if (writer == NULL) return -1;
	return istring_list_push_back(writer->strings, str);
}

// push c string
int icsv_writer_push_cstr(iCsvWriter *writer, const char *ptr, int size)
{
	if (writer == NULL) return -1;
	if (size < 0) size = (int)strlen(ptr);
	return istring_list_push_backc(writer->strings, ptr, size);
}


// utils for reader
int icsv_reader_get_long(const iCsvReader *reader, int i, long *x)
{
	const ivalue_t *src = icsv_reader_get_const(reader, i);
	*x = 0;
	if (src == NULL) return -1;
	*x = istrtol(it_str(src), NULL, 0);
	return 0;
}

int icsv_reader_get_ulong(const iCsvReader *reader, int i, unsigned long *x)
{
	const ivalue_t *src = icsv_reader_get_const(reader, i);
	*x = 0;
	if (src == NULL) return -1;
	*x = istrtoul(it_str(src), NULL, 0);
	return 0;
}

int icsv_reader_get_int(const iCsvReader *reader, int i, int *x)
{
	const ivalue_t *src = icsv_reader_get_const(reader, i);
	*x = 0;
	if (src == NULL) return -1;
	*x = (int)istrtol(it_str(src), NULL, 0);
	return 0;
}

int icsv_reader_get_uint(const iCsvReader *reader, int i, unsigned int *x)
{
	const ivalue_t *src = icsv_reader_get_const(reader, i);
	*x = 0;
	if (src == NULL) return -1;
	*x = (unsigned int)istrtoul(it_str(src), NULL, 0);
	return 0;
}

int icsv_reader_get_int64(const iCsvReader *reader, int i, IINT64 *x)
{
	const ivalue_t *src = icsv_reader_get_const(reader, i);
	*x = 0;
	if (src == NULL) return -1;
	*x = istrtoll(it_str(src), NULL, 0);
	return 0;
}

int icsv_reader_get_uint64(const iCsvReader *reader, int i, IUINT64 *x)
{
	const ivalue_t *src = icsv_reader_get_const(reader, i);
	*x = 0;
	if (src == NULL) return -1;
	*x = istrtoull(it_str(src), NULL, 0);
	return 0;
}

int icsv_reader_get_float(const iCsvReader *reader, int i, float *x)
{
	const ivalue_t *src = icsv_reader_get_const(reader, i);
	*x = 0.0f;
	if (src == NULL) return -1;
	sscanf(it_str(src), "%f", x);
	return 0;
}

int icsv_reader_get_double(const iCsvReader *reader, int i, double *x)
{
	float vv;
	const ivalue_t *src = icsv_reader_get_const(reader, i);
	*x = 0.0;
	if (src == NULL) return -1;
	sscanf(it_str(src), "%f", &vv);
	*x = vv;
	return 0;
}


// utils for writer
int icsv_writer_push_long(iCsvWriter *writer, long x, int radix)
{
	char digit[32];
	if (radix == 10 || radix == 0) {
		iltoa(x, digit, 10);
	}
	else if (radix == 16) {
		return icsv_writer_push_ulong(writer, (unsigned long)x, 16);
	}
	return icsv_writer_push_cstr(writer, digit, -1);
}


int icsv_writer_push_ulong(iCsvWriter *writer, unsigned long x, int radix)
{
	char digit[32];
	if (radix == 10 || radix == 0) {
		iultoa(x, digit, 10);
	}
	else if (radix == 16) {
		digit[0] = '0';
		digit[1] = 'x';
		iultoa(x, digit + 2, 16);
	}
	return icsv_writer_push_cstr(writer, digit, -1);
}

int icsv_writer_push_int(iCsvWriter *writer, int x, int radix)
{
	return icsv_writer_push_long(writer, (long)x, radix);
}

int icsv_writer_push_uint(iCsvWriter *writer, unsigned int x, int radix)
{
	return icsv_writer_push_ulong(writer, (unsigned long)x, radix);
}

int icsv_writer_push_int64(iCsvWriter *writer, IINT64 x, int radix)
{
	char digit[32];
	if (radix == 10 || radix == 0) {
		illtoa(x, digit, 10);
	}
	else if (radix == 16) {
		return icsv_writer_push_uint64(writer, (IUINT64)x, 16);
	}
	return icsv_writer_push_cstr(writer, digit, -1);
}

int icsv_writer_push_uint64(iCsvWriter *writer, IUINT64 x, int radix)
{
	char digit[32];
	if (radix == 10 || radix == 0) {
		iulltoa(x, digit, 10);
	}
	else if (radix == 16) {
		digit[0] = '0';
		digit[1] = 'x';
		iulltoa(x, digit + 2, 16);
	}
	return icsv_writer_push_cstr(writer, digit, -1);
}

int icsv_writer_push_float(iCsvWriter *writer, float x)
{
	char digit[32];
	sprintf(digit, "%f", (float)x);
	return icsv_writer_push_cstr(writer, digit, -1);
}

int icsv_writer_push_double(iCsvWriter *writer, double x)
{
	char digit[32];
	sprintf(digit, "%f", (float)x);
	return icsv_writer_push_cstr(writer, digit, -1);
}


