/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2017 AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
/* ============================ [ INCLUDES  ] ====================================================== */
#include <sys/types.h>
#include <sys/queue.h>
#include <pthread.h>
#include <signal.h>
#include "lasdevlib.h"
#include "afb-wsj1.h"
#include "asocket.h"
#include "asdebug.h"

/* ============================ [ MACROS    ] ====================================================== */
#define PPARAM(p) ((LAS_WebsockParamType*)p)
#define AS_LOG_LAS_DEV 0

#define LAS_WEBSOCK_SERVER 1
#define LAS_WEBSOCK_CLIENT 0
#define LAS_WEBSOCK_ACCEPT 0xacceecca

#define LAS_WS_NAME(s) (((s)==LAS_WEBSOCK_SERVER)?"SERVER":((s)==LAS_WEBSOCK_CLIENT)?"CLIENT":((s)==LAS_WEBSOCK_ACCEPT)?"SRVACCEPT":"UNKNOWN")

#define LAS_CALL 2
#define LAS_REPLY 3
#define LAS_HANGUP 4
#define LAS_EVENT 5

/* ============================ [ TYPES     ] ====================================================== */
struct LAS_WebsockParamStruct;
typedef struct LAS_MessgaeStruct {
	int type;
	char* api;
	char* verb;
	char* event;
	struct afb_wsj1_msg* msg;
	struct LAS_WebsockParamStruct* param;
	STAILQ_ENTRY(LAS_MessgaeStruct) next;
} LAS_MessgaeType;

typedef struct LAS_WebsockParamStruct {
	char* uri;
	int port;
	int s;
	int is_server;
	pthread_t thread; /* server thread to accept client connection */
	struct afb_wsj1 * wsj1;
	struct LAS_WebsockParamStruct *pp;
	STAILQ_ENTRY(LAS_WebsockParamStruct) centry;
	STAILQ_ENTRY(LAS_WebsockParamStruct) sentry;
	STAILQ_ENTRY(LAS_WebsockParamStruct) aentry;	
	STAILQ_HEAD(,LAS_WebsockParamStruct) shead;
	STAILQ_HEAD(,LAS_MessgaeStruct) msgL;
	pthread_mutex_t mutex;
} LAS_WebsockParamType;
/* ============================ [ DECLARES  ] ====================================================== */
static int lasdev_open  (const char* device, const char* option, void** param);
static int lasdev_read  (void* param, char** pdata);
static int lasdev_write (void* param,const char* data,size_t size);
static void lasdev_close(void* param);
static int lasdev_ioctl (void* param,int type, const char* data,size_t size);
 
static void on_hangup(void *closure, struct afb_wsj1 *wsj1);
static void on_call(void *closure, const char *api, const char *verb, struct afb_wsj1_msg *msg);
static void on_event(void *closure, const char *event, struct afb_wsj1_msg *msg);
static void on_reply(void *closure, struct afb_wsj1_msg *msg);
static void* server_main(void* param);

/* ============================ [ DATAS     ] ====================================================== */
const LAS_DeviceOpsType websock_dev_ops = {
	.name = "websock",
	.open = lasdev_open,
	.read = lasdev_read,
	.write = lasdev_write,
	.close = lasdev_close,
	.ioctl = lasdev_ioctl
};
static const struct afb_wsj1_itf wsj1_itf =
{
	.on_hangup = on_hangup,
	.on_call = on_call,
	.on_event = on_event
};
pthread_mutex_t scMutex = PTHREAD_MUTEX_INITIALIZER;
static STAILQ_HEAD(,LAS_WebsockParamStruct) serverHead = STAILQ_HEAD_INITIALIZER(serverHead);
static STAILQ_HEAD(,LAS_WebsockParamStruct) clientHead = STAILQ_HEAD_INITIALIZER(clientHead);
/* ============================ [ LOCALS    ] ====================================================== */
static int lasdev_open  (const char* device, const char* option, void** param)
{
	char* uri;
	char* p;
	int s,port,is_server,is_num,n;
	/* option format "uri\0port\0server", e.g. "127.0.0.1\08080\01" */
	uri = option;
	p = uri + strlen(uri)+1;
	port = atoi(p);
	p = p + strlen(p)+1;
	is_server = atoi(p);

	s = ask_create(is_server,uri,port);
	if(s < 0)
	{
		ASWARNING("create websock %s on %s:%d failed at function '%s'\n",
				is_server?"server":"client",uri,port,__func__);

		return 0;
	}

	*param = malloc(sizeof(LAS_WebsockParamType));
	asAssert(*param);
	PPARAM(*param)->s = s;
	PPARAM(*param)->port = port;
	PPARAM(*param)->uri = strdup(uri);
	PPARAM(*param)->is_server = is_server;

#ifdef __WINDOWS__
	PPARAM(*param)->mutex = PTHREAD_MUTEX_INITIALIZER;
#else
	pthread_mutex_init(&PPARAM(*param)->mutex, NULL);
#endif

	STAILQ_INIT(&PPARAM(*param)->msgL);

	if(is_server)
	{
		STAILQ_INIT(&PPARAM(*param)->shead);

		PPARAM(*param)->wsj1 = NULL;
		PPARAM(*param)->pp = NULL;

		pthread_mutex_lock(&scMutex);
		STAILQ_INSERT_TAIL(&serverHead,PPARAM(*param),sentry);
		pthread_mutex_unlock(&scMutex);

		pthread_create(&(PPARAM(*param)->thread),NULL,server_main,*param);
	}
	else
	{
		PPARAM(*param)->wsj1 = afb_wsj1_create(s,&wsj1_itf,*param);
		PPARAM(*param)->pp = NULL;

		pthread_mutex_lock(&scMutex);
		STAILQ_INSERT_TAIL(&clientHead,PPARAM(*param),centry);
		pthread_mutex_unlock(&scMutex);
	}

	ASLOG(LAS_DEV,"websock %s<%d> online %s:%d\n",LAS_WS_NAME(PPARAM(*param)->is_server),PPARAM(*param)->s,PPARAM(*param)->uri,PPARAM(*param)->port);
	return 1;
}
static int lasdev_read  (void* param,char** pdata)
{
	char *data = NULL;;
	char* format;
	int len = 0;
	LAS_WebsockParamType* p;
	LAS_WebsockParamType* tp = NULL;
	if(LAS_WEBSOCK_SERVER == PPARAM(param)->is_server)
	{
		pthread_mutex_lock(&PPARAM(param)->mutex);
		STAILQ_FOREACH(p,&PPARAM(param)->shead,aentry)
		{
			if(!STAILQ_EMPTY(&p->msgL))
			{
				tp = p;
				break;
			}
		}
		pthread_mutex_unlock(&PPARAM(param)->mutex);
	}
	else
	{
		tp = param;
	}

	if(tp == NULL)
	{
		return 0;
	}

	pthread_mutex_lock(&tp->mutex);
	if(!STAILQ_EMPTY(&tp->msgL))
	{
		LAS_MessgaeType* msg = STAILQ_FIRST(&tp->msgL);
		switch(msg->type)
		{
			case LAS_CALL:
				format = "{ \"api\":\"%s\",\"verb\":\"%s\",\"obj\":\"%s\",\"param\":\"%p\" }";
				len = strlen(msg->api)+strlen(msg->verb)+strlen(afb_wsj1_msg_object_s(msg->msg))+strlen(format)-8+1+sizeof(void*)*2+2;
				data = malloc(len);
				asAssert(data);
				len = snprintf(data,len,format,msg->api,msg->verb,afb_wsj1_msg_object_s(msg->msg),tp);
				break;
			case LAS_EVENT:
				format = "{ \"event\":\"%s\",\"obj\":\"%s\"}";
				len = strlen(msg->event)+strlen(afb_wsj1_msg_object_s(msg->msg))+strlen(format)-4+1;
				data = malloc(len);
				asAssert(data);
				len = snprintf(data,len,format,msg->event,afb_wsj1_msg_object_s(msg->msg));
				break;
			case LAS_REPLY:
				format = "{ \"reply\":\"?\",\"obj\":\"%s\"}";
				len = strlen(afb_wsj1_msg_object_s(msg->msg))+strlen(format)-2+1;
				data = malloc(len);
				asAssert(data);
				len = snprintf(data,len,format,afb_wsj1_msg_object_s(msg->msg));
				break;
			default:
				asAssert(0);
				break;
		}

		STAILQ_REMOVE_HEAD(&tp->msgL,next);
		afb_wsj1_msg_unref(msg->msg);
		free(msg);
	}
	pthread_mutex_unlock(&tp->mutex);
	*pdata = data;
	return len;
}
static int lasdev_write (void* param,const char* data,size_t size)
{
	int err=0;
	int len;
	const char* api;
	const char* verb;
	const char* obj;
	if(size > 3)
	{
		switch(data[0])
		{
			case 'c':
				api = &data[1];
				len = strlen(api)+2;
				verb = &data[len];
				len += strlen(verb)+1;
				obj = &data[len];
				if(PPARAM(param)->wsj1)
				{
					err = afb_wsj1_call_s(PPARAM(param)->wsj1, api, verb, obj, on_reply,param);
				}
				else
				{
					ASWARNING("%s websock call on server is not supported\n",__func__);
					err = -3;					
				}
				break;
			default:
				ASWARNING("%s websock wrong data format\n",__func__);
				err = -2;
				break;
		}
	}
	else
	{
		ASWARNING("%s websock wrong data size\n",__func__);
		err = -1;
	}
	return err;
}
static void lasdev_close(void* param)
{
	ASLOG(LAS_DEV,"websock close %s<%d> on %s:%d\n",LAS_WS_NAME(PPARAM(param)->is_server),PPARAM(param)->s,PPARAM(param)->uri,PPARAM(param)->port);

	if(LAS_WEBSOCK_SERVER == PPARAM(param)->is_server)
	{
		LAS_WebsockParamType *p;
		PPARAM(param)->is_server = 0;
#ifdef __WINDOWS__
		Sleep(1);
#else
		usleep(1000);
#endif
		/* sleep to make sure the server thread exit */
		pthread_mutex_lock(&scMutex);
		STAILQ_REMOVE(&serverHead,PPARAM(param),LAS_WebsockParamStruct,sentry);
		pthread_mutex_unlock(&scMutex);
		pthread_mutex_lock(&PPARAM(param)->mutex);
		while(FALSE == STAILQ_EMPTY(&PPARAM(param)->shead))
		{
			p = STAILQ_FIRST(&PPARAM(param)->shead);
			pthread_mutex_unlock(&PPARAM(param)->mutex);
			lasdev_close(p);
			pthread_mutex_lock(&PPARAM(param)->mutex);
		}
		pthread_mutex_unlock(&PPARAM(param)->mutex);
	}
	else if(LAS_WEBSOCK_CLIENT == PPARAM(param)->is_server)
	{
		pthread_mutex_lock(&scMutex);
		STAILQ_REMOVE(&clientHead,PPARAM(param),LAS_WebsockParamStruct,centry);
		pthread_mutex_unlock(&scMutex);
	}
	else
	{
		LAS_WebsockParamType *pp = PPARAM(param)->pp;
		pthread_mutex_lock(&pp->mutex);
		STAILQ_REMOVE(&pp->shead,PPARAM(param),LAS_WebsockParamStruct,aentry);
		pthread_mutex_unlock(&pp->mutex);
	}

	close(PPARAM(param)->s);
	free(param);
}
static int lasdev_ioctl (void* param,int type, const char* data,size_t size)
{
	return 0;
}
 
static void on_hangup(void *closure, struct afb_wsj1 *wsj1)
{
	LAS_WebsockParamType *param=(LAS_WebsockParamType*)closure;
	LAS_MessgaeType *m = malloc(sizeof(LAS_MessgaeType));
	ASLOG(LAS_DEV,"ON-HANGUP on %s<%d> %s:%d\n",LAS_WS_NAME(param->is_server),param->s,param->uri,param->port);

	asAssert(m);
	m->type = LAS_HANGUP;
	pthread_mutex_lock(&param->mutex);
	STAILQ_INSERT_TAIL(&param->msgL,m,next);
	pthread_mutex_unlock(&param->mutex);
}
static void on_call(void *closure, const char *api, const char *verb, struct afb_wsj1_msg *msg)
{
	LAS_WebsockParamType *param=(LAS_WebsockParamType*)closure;
	LAS_MessgaeType *m = malloc(sizeof(LAS_MessgaeType));

	ASLOG(LAS_DEV,"ON-CALL %s/%s(%s) on %s<%d> %s:%d\n", api, verb, afb_wsj1_msg_object_s(msg),LAS_WS_NAME(param->is_server),param->s,param->uri,param->port);

	asAssert(m);
	m->type = LAS_CALL;
	m->api = api;
	m->verb = verb;
	m->msg = msg;
	pthread_mutex_lock(&param->mutex);
	STAILQ_INSERT_TAIL(&param->msgL,m,next);
	pthread_mutex_unlock(&param->mutex);
}
static void on_event(void *closure, const char *event, struct afb_wsj1_msg *msg)
{
	LAS_WebsockParamType *param=(LAS_WebsockParamType*)closure;
	LAS_MessgaeType *m = malloc(sizeof(LAS_MessgaeType));
	ASLOG(LAS_DEV,"ON-EVENT %s(%s)  on %s<%d> %s:%d\n", event, afb_wsj1_msg_object_s(msg),LAS_WS_NAME(param->is_server),param->s,param->uri,param->port);
	asAssert(m);
	m->type = LAS_EVENT;
	m->event = event;
	m->msg = msg;
	pthread_mutex_lock(&param->mutex);
	STAILQ_INSERT_TAIL(&param->msgL,m,next);
	pthread_mutex_unlock(&param->mutex);
}
static void on_reply(void *closure, struct afb_wsj1_msg *msg)
{
	LAS_WebsockParamType *param=(LAS_WebsockParamType*)closure;
	LAS_MessgaeType *m = malloc(sizeof(LAS_MessgaeType));
	ASLOG(LAS_DEV,"ON-REPLY : %s  on %s<%d> %s:%d\n",afb_wsj1_msg_object_s(msg),LAS_WS_NAME(param->is_server),param->s,param->uri,param->port);
	asAssert(m);
	m->type = LAS_REPLY;
	m->msg = msg;
	pthread_mutex_lock(&param->mutex);
	STAILQ_INSERT_TAIL(&param->msgL,m,next);
	pthread_mutex_unlock(&param->mutex);
}

static void* server_main(void* param)
{
	int s;
	LAS_WebsockParamType *sp;
	while(PPARAM(param)->is_server)
	{
		s = ask_accept(PPARAM(param)->s);
		if(s > 0)
		{
			sp = malloc(sizeof(LAS_WebsockParamType));
			asAssert(sp);
			memcpy(sp,param,sizeof(LAS_WebsockParamType));
			sp->pp=param;
			sp->s = s;
			sp->is_server = LAS_WEBSOCK_ACCEPT;
			#ifdef __WINDOWS__
			sp->mutex = PTHREAD_MUTEX_INITIALIZER;
			#else
			pthread_mutex_init(&sp->mutex, NULL);
			#endif
			STAILQ_INIT(&sp->msgL);
			sp->wsj1 = afb_wsj1_create(s,&wsj1_itf,sp);
			pthread_mutex_lock(&PPARAM(param)->mutex);
			STAILQ_INSERT_TAIL(&PPARAM(param)->shead,sp,aentry);
			pthread_mutex_unlock(&PPARAM(param)->mutex);
		}
	}

	return 0;
}
/* ============================ [ FUNCTIONS ] ====================================================== */
