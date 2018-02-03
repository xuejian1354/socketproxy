/*
 * netapi.h
 *
 * Copyright (C) 2013 loongsky development.
 *
 * Sam Chen <xuejian1354@163.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __NETAPI_H__
#define __NETAPI_H__

#include "globals.h"
#include <sys/socket.h>
#include <arpa/inet.h>

#define RECV_PACK_START	'$'

#define RECV_CMD_OPEN		0
#define RECV_CMD_MESSAGE	1
#define RECV_CMD_CLOSE		2
#define RECV_CMD_MAC		3
#define RECV_CMD_HEART		4

enum RecvActionEU
{
	RECV_ACTION_START,
	RECV_ACTION_CMD,
	RECV_ACTION_ADDRLEN,
	RECV_ACTION_DATALEN,
	RECV_ACTION_ADDR,
	RECV_ACTION_DATA
};

int get_ctcp_fd();
int socket_tcp_client_connect(char *host, int port);
int socket_tcp_client_recv(int fd);

#endif  // __NETAPI_H__
