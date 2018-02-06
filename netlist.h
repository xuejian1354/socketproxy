/*
 * netlist.h
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
#ifndef __NETLIST_H__
#define __NETLIST_H__

#include "globals.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define TRANS_TCP_CONN_MAX_SIZE	65536

typedef struct TCPConn
{
	int fd;
	int isconnect;
	int port;
	char host_addr[64];
	int host_port;
	struct sockaddr_in host_in;
	void *extdata;
	struct TCPConn *next;
}tcp_conn_t;

typedef struct TCPConnList
{
	tcp_conn_t *p_head;
	int num;
	const int max_size;
}tcp_conn_list_t;

tcp_conn_list_t *get_tcp_conn_list();
tcp_conn_t *new_tcpconn(int fd, int isconnect, 
	int port, char *host_addr, int host_port, void *extdata);
int addto_tcpconn_list(tcp_conn_t *list);
tcp_conn_t *queryfrom_tcpconn_list(int fd);
tcp_conn_t *queryfrom_tcpconn_list_with_localport(int port);
int delfrom_tcpconn_list(int fd);
#endif  // __NETLIST_H__