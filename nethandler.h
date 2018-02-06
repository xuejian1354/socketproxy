/*
 * nethandler.h
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
#ifndef __NETHANDLER_H__
#define __NETHANDLER_H__

#include "globals.h"
#include "netlist.h"

enum ConnStats
{
	CONN_STAT_WORK,
	CONN_STAT_SLEEP
};

enum ConnWay
{
	CONN_WITH_SERVER = 0,
	CONN_WITH_CLIENT
};

typedef struct ExtConnData
{
	tcp_conn_t *toconn;
	enum ConnWay way;
	int is_time_count;
	struct timeval temp;
}ext_conn_t;

struct timespec *get_timespec();

int net_tcp_connect();
int net_tcp_recv(int fd);

void time_handler();

#endif  // __NETHANDLER_H__

