/*
 * nethandler.h
 */
#ifndef __NETHANDLER_H__
#define __NETHANDLER_H__

#include "globals.h"
#include "netlist.h"

enum ConnWay
{
	CONN_WITH_SERVER = 0,
	CONN_WITH_CLIENT
};

typedef struct ExtConnData
{
	tcp_conn_t *toconn;
	enum ConnWay way;
	int isuse;
}ext_conn_t;

struct timespec *get_timespec();

int net_tcp_connect();
int net_tcp_recv(int fd);

void time_handler();

#endif  // __NETHANDLER_H__

