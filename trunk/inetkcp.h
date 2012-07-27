//=====================================================================
//
// inetkcp.h - fast ARQ protocol implementation
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================
#ifndef __INETKCP_H__
#define __INETKCP_H__

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "imemdata.h"



//---------------------------------------------------------------------
// segment
//---------------------------------------------------------------------
struct IKCPSEG
{
	struct IQUEUEHEAD node;
	IUINT32 conv;
	IUINT32 cmd;
	IUINT32 frg;
	IUINT32 wnd;
	IUINT32 ts;
	IUINT32 sn;
	IUINT32 una;
	IUINT32 len;
	IUINT32 resendts;
	IUINT32 rto;
	IUINT32 fastack;
	IUINT32 xmit;
	char data[1];
};


//---------------------------------------------------------------------
// IKCPCB
//---------------------------------------------------------------------
struct IKCPCB
{
	IUINT32 conv, mtu, mss, state;
	IUINT32 snd_una, snd_nxt, rcv_nxt;
	IUINT32 ts_recent, ts_lastack;
	IINT32 rx_rttval, rx_srtt, rx_rto, rx_minrto;
	IUINT32 snd_wnd, rcv_wnd, rmt_wnd, ask_wnd;
	IUINT32 current, interval, ts_flush;
	IUINT32 nrcv_buf, nsnd_buf;
	IUINT32 nrcv_que, nsnd_que;
	IUINT32 nodelay, updated;
	IUINT32 ts_probe, probe_wait;
	struct IQUEUEHEAD snd_queue;
	struct IQUEUEHEAD rcv_queue;
	struct IQUEUEHEAD snd_buf;
	struct IQUEUEHEAD rcv_buf;
	ivector_t *acklist;
	void *user;
	char *buffer;
	int fastresend;
	int logmask;
	int (*output)(const char *buf, int len, struct IKCPCB *kcp, void *user);
	void (*writelog)(const char *log, struct IKCPCB *kcp, void *user);
};


typedef struct IKCPCB ikcpcb;

#define IKCP_LOG_OUTPUT			1
#define IKCP_LOG_INPUT			2
#define IKCP_LOG_SEND			4
#define IKCP_LOG_RECV			8
#define IKCP_LOG_IN_DATA		16
#define IKCP_LOG_IN_ACK			32
#define IKCP_LOG_IN_PROBE		64
#define IKCP_LOG_IN_WINS		128
#define IKCP_LOG_OUT_DATA		256
#define IKCP_LOG_OUT_ACK		512
#define IKCP_LOG_OUT_PROBE		1024
#define IKCP_LOG_OUT_WINS		2048

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------
// interface
//---------------------------------------------------------------------

ikcpcb* ikcp_create(IUINT32 conv, void *user);
void ikcp_release(ikcpcb *kcp);

int ikcp_recv(ikcpcb *kcp, char *buffer, int len);
int ikcp_send(ikcpcb *kcp, const char *buffer, int len);

void ikcp_update(ikcpcb *kcp, IUINT32 current);
IUINT32 ikcp_check(const ikcpcb *kcp, IUINT32 current);

int ikcp_input(ikcpcb *kcp, const char *data, long size);
void ikcp_flush(ikcpcb *kcp);

int ikcp_peeksize(const ikcpcb *kcp);

int ikcp_setmtu(ikcpcb *kcp, int mtu);
int ikcp_nodelay(ikcpcb *kcp, int nodelay, int interval, int resend);
int ikcp_wndsize(ikcpcb *kcp, int sndwnd, int rcvwnd);
int ikcp_waitsnd(const ikcpcb *kcp);

int ikcp_rcvbuf_count(const ikcpcb *kcp);
int ikcp_sndbuf_count(const ikcpcb *kcp);

void ikcp_log(ikcpcb *kcp, int mask, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif


