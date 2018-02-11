/*
 * netlist.h
 */
#ifndef __NETLIST_H__
#define __NETLIST_H__

#include "mtypes.h"
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
void clear_all_conn(void (*del_call)(tcp_conn_t *));
#endif  // __NETLIST_H__