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

//#define IDISABLE_FILE_SYSTEM_ACCESS
//---------------------------------------------------------------------
// System Compatible
//---------------------------------------------------------------------
#ifndef IDISABLE_FILE_SYSTEM_ACCESS
#define IENABLE_FILE_SYSTEM_ACCESS			// 是否允许文件系统访问
#endif

#ifndef IDISABLE_SHARED_LIBRARY
#define IENABLE_SHARED_LIBRARY				// 是否允许动态库
#endif

#ifndef IUTILS_STACK_BUFFER_SIZE
#define IUTILS_STACK_BUFFER_SIZE	1024	// 默认栈缓存
#endif

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
// System Utilities
//---------------------------------------------------------------------

#ifdef IENABLE_SHARED_LIBRARY

// LoadLibraryA
void *iutils_shared_open(const char *dllname);

// GetProcAddress
void *iutils_shared_get(void *shared, const char *name);

// FreeLibrary
void iutils_shared_close(void *shared);

#endif


#ifdef IENABLE_FILE_SYSTEM_ACCESS

// load file content
void *iutils_file_load_content(const char *filename, ilong *size);

// load file content
int iutils_file_load_to_str(const char *filename, ivalue_t *str);

// load line: returns -1 for end of file, 0 for success
int iutils_file_read_line(FILE *fp, ivalue_t *str);

// cross os GetModuleFileName, returns size for success, -1 for error
int iutils_get_proc_pathname(char *ptr, int size);

#endif


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
// Posix Stat
//---------------------------------------------------------------------
#ifdef IENABLE_FILE_SYSTEM_ACCESS

#define ISTAT_IFMT		0170000		// file type mask
#define ISTAT_IFIFO		0010000		// named pipe (fifo) 
#define ISTAT_IFCHR		0020000		// charactor special
#define ISTAT_IFDIR		0040000		// directory
#define ISTAT_IFBLK		0060000		// block special
#define ISTAT_IFREG		0100000		// regular
#define ISTAT_IFLNK		0120000		// symbolic link
#define ISTAT_IFSOCK	0140000		// socket
#define ISTAT_IFWHT		0160000		// whiteout
#define ISTAT_ISUID		0004000		// set user id on execution
#define ISTAT_ISGID		0002000		// set group id on execution
#define ISTAT_ISVXT		0001000		// swapped text even after use
#define ISTAT_IRWXU		0000700		// owner RWX mask
#define ISTAT_IRUSR		0000400		// owner read permission
#define ISTAT_IWUSR		0000200		// owner writer permission
#define ISTAT_IXUSR		0000100		// owner execution permission
#define ISTAT_IRWXG		0000070		// group RWX mask
#define ISTAT_IRGRP		0000040		// group read permission
#define ISTAT_IWGRP		0000020		// group write permission
#define ISTAT_IXGRP		0000010		// group execution permission
#define ISTAT_IRWXO		0000007		// other RWX mask
#define ISTAT_IROTH		0000004		// other read permission
#define ISTAT_IWOTH		0000002		// other writer permission
#define ISTAT_IXOTH		0000001		// other execution permission

#define ISTAT_ISFMT(m, t)	(((m) & ISTAT_IFMT) == (t))
#define ISTAT_ISDIR(m)		ISTAT_ISFMT(m, ISTAT_IFDIR)
#define ISTAT_ISCHR(m)		ISTAT_ISFMT(m, ISTAT_IFCHR)
#define ISTAT_ISBLK(m)		ISTAT_ISFMT(m, ISTAT_IFBLK)
#define ISTAT_ISREG(m)		ISTAT_ISFMT(m, ISTAT_IFREG)
#define ISTAT_ISFIFO(m)		ISTAT_ISFMT(m, ISTAT_IFIFO)
#define ISTAT_ISLNK(m)		ISTAT_ISFMT(m, ISTAT_IFLNK)
#define ISTAT_ISSOCK(m)		ISTAT_ISFMT(m, ISTAT_IFSOCK)
#define ISTAT_ISWHT(m)		ISTAT_ISFMT(m, ISTAT_IFWHT)

struct IPOSIX_STAT
{
	IUINT32 st_mode;
	IUINT64 st_ino;
	IUINT32 st_dev;
	IUINT32 st_nlink;
	IUINT32 st_uid;
	IUINT32 st_gid;
	IUINT64 st_size;
	IUINT32 atime;
	IUINT32 mtime;
	IUINT32 ctime;
	IUINT32 st_blocks;
	IUINT32 st_blksize;
	IUINT32 st_rdev;
	IUINT32 st_flags;
};

typedef struct IPOSIX_STAT iposix_stat_t;

#define IPOSIX_MAXPATH		1024
#define IPOSIX_MAXBUFF		((IPOSIX_MAXPATH) + 8)


// returns 0 for success, -1 for error
int iposix_stat(const char *path, iposix_stat_t *ostat);

// returns 0 for success, -1 for error
int iposix_lstat(const char *path, iposix_stat_t *ostat);

// returns 0 for success, -1 for error
int iposix_fstat(int fd, iposix_stat_t *ostat);

// get current directory
char *iposix_getcwd(char *path, int size);

// create directory
int iposix_mkdir(const char *path, int mode);


// returns 1 for true 0 for false, -1 for not exist
int iposix_path_isdir(const char *path);

// returns 1 for true 0 for false, -1 for not exist
int iposix_path_isfile(const char *path);

// returns 1 for true 0 for false, -1 for not exist
int iposix_path_islink(const char *path);

// returns 1 for true 0 for false
int iposix_path_exists(const char *path);

// returns file size, -1 for error
IINT64 iposix_path_getsize(const char *path);

#endif


//---------------------------------------------------------------------
// CSV Reader/Writer
//---------------------------------------------------------------------
struct iCsvReader;
struct iCsvWriter;

typedef struct iCsvReader iCsvReader;
typedef struct iCsvWriter iCsvWriter;


// open csv reader from file 
iCsvReader *icsv_reader_open_file(const char *filename);

// open csv reader from memory 
iCsvReader *icsv_reader_open_memory(const char *text, ilong size);

// close csv reader
void icsv_reader_close(iCsvReader *reader);

// read csv row
int icsv_reader_read(iCsvReader *reader);

// get column count in current row
int icsv_reader_size(const iCsvReader *reader);

// returns 1 for end of file, 0 for not end.
int icsv_reader_eof(const iCsvReader *reader);

// get column string
ivalue_t *icsv_reader_get(iCsvReader *reader, int pos);

// get column string
const ivalue_t *icsv_reader_get_const(const iCsvReader *reader, int pos);

// return column string size, -1 for error
int icsv_reader_get_size(const iCsvReader *reader, int pos);

// return column string, returns string size for success, -1 for error
int icsv_reader_get_string(const iCsvReader *reader, int pos, ivalue_t *out);

// return column string, returns string size for success, -1 for error
int icsv_reader_get_cstr(const iCsvReader *reader, int pos, 
	char *out, int size);

// utils for reader
int icsv_reader_get_long(const iCsvReader *reader, int i, long *x);
int icsv_reader_get_ulong(const iCsvReader *reader, int i, unsigned long *x);
int icsv_reader_get_int(const iCsvReader *reader, int i, int *x);
int icsv_reader_get_uint(const iCsvReader *reader, int i, unsigned int *x);
int icsv_reader_get_int64(const iCsvReader *reader, int i, IINT64 *x);
int icsv_reader_get_uint64(const iCsvReader *reader, int i, IUINT64 *x);
int icsv_reader_get_float(const iCsvReader *reader, int i, float *x);
int icsv_reader_get_double(const iCsvReader *reader, int i, double *x);


// open csv writer from file: if filename is NULL, it will open in memory
iCsvWriter *icsv_writer_open(const char *filename, int append);

// close csv writer
void icsv_writer_close(iCsvWriter *writer);

// write row and reset
int icsv_writer_write(iCsvWriter *writer);

// return column count in current row
int icsv_writer_size(iCsvWriter *writer);

// clear columns in current row
void icsv_writer_clear(iCsvWriter *writer);

// dump output
void icsv_writer_dump(iCsvWriter *writer, ivalue_t *out);

// clear output
void icsv_writer_empty(iCsvWriter *writer);

// push string
int icsv_writer_push(iCsvWriter *writer, const ivalue_t *str);

// push c string
int icsv_writer_push_cstr(iCsvWriter *writer, const char *ptr, int size);


// utils for writer
int icsv_writer_push_long(iCsvWriter *writer, long x, int radix);
int icsv_writer_push_ulong(iCsvWriter *writer, unsigned long x, int radix);
int icsv_writer_push_int(iCsvWriter *writer, int x, int radix);
int icsv_writer_push_uint(iCsvWriter *writer, unsigned int x, int radix);
int icsv_writer_push_int64(iCsvWriter *writer, IINT64 x, int radix);
int icsv_writer_push_uint64(iCsvWriter *writer, long x, int radix);
int icsv_writer_push_float(iCsvWriter *writer, float x);
int icsv_writer_push_double(iCsvWriter *writer, double x);

#ifdef __cplusplus
}
#endif



#endif



