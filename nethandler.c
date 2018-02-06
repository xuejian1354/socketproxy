/*
 * nethandler.c
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

#include "nethandler.h"
#include "netlist.h"
#include <errno.h>


static int nbytes;
static char buf[MAXSIZE];

void data_handler(int fd, char *data, int len);

uint32 get_socket_local_port(int fd)
{
	struct sockaddr_in loc_addr;
	int len = sizeof(sizeof(loc_addr));
	memset(&loc_addr, 0, len);
	if (getsockname(fd, (struct sockaddr *)&loc_addr, &len) == 0
		&& (loc_addr.sin_family == AF_INET)) {
		return ntohs(loc_addr.sin_port);
	}

	return 0;
}

tcp_conn_t *try_connect(char *host, int port, enum ConnWay way)
{
	int fd;
	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("client tcp socket fail");
		return NULL;
	}

	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags|O_NONBLOCK);

	ext_conn_t *extdata = calloc(1, sizeof(ext_conn_t));
	extdata->way = way;
	tcp_conn_t *tconn = new_tcpconn(fd, 0, 0, host, port, extdata);
	if(tconn == NULL)
	{
		free(extdata);
		return NULL;
	}

	int res = connect(fd, (struct sockaddr *)&tconn->host_in, sizeof(tconn->host_in));
	if(0 != res && errno != EINPROGRESS)
	{
		perror("client tcp socket connect server fail");
		AO_PRINTF("[%s] connect %s fail\n", get_current_time(), host);
		return NULL;
	}

	if(res == 0)
	{
		tconn->isconnect = 1;
		tconn->port = get_socket_local_port(fd);
	}

	addto_tcpconn_list(tconn);
	select_wtset(fd);
	return tconn;
}


int net_tcp_connect(char *host, int port)
{
	int i;
	for(i=0; i<SERVER_TCPLINK_NUM; i++)
	{
		try_connect(get_host_addr(), get_host_port(), CONN_WITH_SERVER);
	}

	return 0;
}

int net_tcp_recv(int fd)
{
	tcp_conn_t *t_conn = queryfrom_tcpconn_list(fd);
	if(t_conn == NULL)
	{
		return 0;
	}

	if(t_conn->isconnect)
	{
		memset(buf, 0, sizeof(buf));
	   	if ((nbytes = recv(fd, buf, sizeof(buf), 0)) <= 0)
	   	{
			tcp_conn_t *toconn = ((ext_conn_t *)(t_conn->extdata))->toconn;
			if(toconn && toconn->isconnect)
			{
				AI_PRINTF("[%s] close, fd=%d\n", get_current_time(), toconn->fd);
				close(toconn->fd);
				select_clr(toconn->fd);
				delfrom_tcpconn_list(toconn->fd);
			}
			else
			{
				((ext_conn_t *)(toconn->extdata))->toconn = NULL;
			}

			AI_PRINTF("[%s] close, fd=%d\n", get_current_time(), fd);
			close(fd);
			select_clr(fd);
			delfrom_tcpconn_list(fd);
		}
		else
		{
			//PRINT_HEX(buf, nbytes);
			data_handler(fd, buf, nbytes);
		}
	}
	else
	{
		int errinfo, errlen;
		errlen = sizeof(errlen);

        if (0 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &errinfo, &errlen))    
        {
			t_conn->isconnect = 1;
			t_conn->port = get_socket_local_port(fd);
			select_set(fd);

			AO_PRINTF("[%s] connect %s:%d, current port=%d, fd=%d\n",
				get_current_time(), t_conn->host_addr,
				t_conn->host_port, t_conn->port, fd);

			if(((ext_conn_t *)(t_conn->extdata))->way == CONN_WITH_SERVER)
			{
				tcp_conn_t *cliconn = 
					try_connect("127.0.0.1", get_transport(), CONN_WITH_CLIENT);
				if(cliconn)
				{
					((ext_conn_t *)(t_conn->extdata))->toconn = cliconn;
					((ext_conn_t *)(cliconn->extdata))->toconn = t_conn;
				}
			}
			else if(((ext_conn_t *)(t_conn->extdata))->way == CONN_WITH_CLIENT)
			{
				if(((ext_conn_t *)(t_conn->extdata))->toconn == NULL)
				{
					AI_PRINTF("[%s] close, fd=%d\n", get_current_time(), fd);
					close(fd);
					select_clr(fd);
					delfrom_tcpconn_list(fd);
				}
			}
        }
		else
		{
			AO_PRINTF("[%s] connect %s fail\n",
				get_current_time(), t_conn->host_addr, fd);
		}

		select_wtclr(fd);
	}

	return 0;
}

void data_handler(int fd, char *data, int len)
{
	tcp_conn_t *tconn = queryfrom_tcpconn_list(fd);
	if(tconn && ((ext_conn_t *)tconn->extdata)->toconn)
	{
		send(((ext_conn_t *)tconn->extdata)->toconn->fd, data, len, 0);
	}
	else
	{
		AO_PRINTF("[%s] can't find connection from list, fd=%d\n",
			get_current_time(), fd);
	}
}

