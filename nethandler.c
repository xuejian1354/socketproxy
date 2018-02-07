/*
 * nethandler.c
 */

#include "nethandler.h"
#include "netlist.h"
#include <errno.h>


static int nbytes;
static char buf[MAXSIZE];

struct timespec *select_time = NULL;
struct timespec local_time;

int serlink_count = 0;

void data_handler(int fd, char *data, int len);

int get_rand(int min, int max) {
	return (rand() % (max-min+1)) + min;
}


void set_timespec(time_t s)
{
	if(s == 0)
	{
		select_time = NULL;
	}
	else
	{
		local_time.tv_sec = s;
		local_time.tv_nsec = 0;
		select_time = &local_time;
	}
}

struct timespec *get_timespec()
{
	return select_time;
}

void close_connection(int fd)
{
	close(fd);
	select_clr(fd);
}

void detect_link()
{
	if(serlink_count < get_max_connections_num())
	{
		set_timespec(get_rand(58, 118));
	}
	else if(serlink_count <= 10)
	{
		clear_all_conn(close_connection);
		set_timespec(get_rand(1200, 2400));
	}
	else
	{
		set_timespec(0);
	}
}

void release_connection_with_fd(int fd)
{
	detect_link();
	close_connection(fd);
	delfrom_tcpconn_list(fd);
	AO_PRINTF("[%s] close, fd=%d, total=%d\n",
		get_current_time(), fd, serlink_count);
}

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

	if(way != CONN_WITH_CLIENT)
	{
		int flags = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, flags|O_NONBLOCK);
	}

	ext_conn_t *extdata = calloc(1, sizeof(ext_conn_t));
	extdata->toconn = NULL;
	extdata->way = way;
	extdata->isuse = 0;
	tcp_conn_t *tconn = new_tcpconn(fd, 0, 0, host, port, extdata);
	if(tconn == NULL)
	{
		free(extdata);
		return NULL;
	}

	int res = connect(fd, (struct sockaddr *)&tconn->host_in, sizeof(tconn->host_in));
	if(0 != res && (way == CONN_WITH_CLIENT || errno != EINPROGRESS))
	{
		perror("client tcp socket connect server fail");
		AO_PRINTF("[%s] connect %s fail\n", get_current_time(), host);
		return NULL;
	}

	if(res == 0)
	{
		tconn->isconnect = 1;
		tconn->port = get_socket_local_port(fd);
		if(way != CONN_WITH_CLIENT)
		{
			serlink_count++;
			detect_link();
		}
		AO_PRINTF("[%s] connect %s:%d, current port=%d, fd=%d, total=%d\n",
				get_current_time(), tconn->host_addr,
				tconn->host_port, tconn->port, fd, serlink_count);
	}

	addto_tcpconn_list(tconn);
	if(way != CONN_WITH_CLIENT)
	{
		select_wtset(fd);
	}
	else
	{
		select_set(fd);
	}

	return tconn;
}


int net_tcp_connect(char *host, int port)
{
	int i;
	for(i=0; i<get_max_connections_num(); i++)
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
			int isreconnect = 0;
			ext_conn_t *extdata = (ext_conn_t *)(t_conn->extdata);
			if(extdata)
			{
				tcp_conn_t *toconn = extdata->toconn;
				if(extdata->way == CONN_WITH_SERVER && toconn && toconn->isconnect)
				{
					AO_PRINTF("[%s] line:%d to target\n", get_current_time(), __LINE__);
					release_connection_with_fd(toconn->fd);
					AO_PRINTF("[%s] line:%d to server\n", get_current_time(), __LINE__);
				}
				else
				{
					if(extdata->way == CONN_WITH_SERVER)
					{
						AO_PRINTF("[%s] line:%d to server\n", get_current_time(), __LINE__);
					}
					else
					{
						AO_PRINTF("[%s] line:%d to target\n", get_current_time(), __LINE__);
					}
					toconn = NULL;
				}

				if(extdata->way == CONN_WITH_SERVER)
				{
					serlink_count--;
					if(extdata->isuse)
						isreconnect = 1;
				}
			}

			release_connection_with_fd(fd);
			if(isreconnect)
			{
				try_connect(get_host_addr(), get_host_port(), CONN_WITH_SERVER);
			}
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

			if(((ext_conn_t *)(t_conn->extdata))->way == CONN_WITH_SERVER)
			{
				serlink_count++;
				detect_link();
			}

			AO_PRINTF("[%s] connect %s:%d, current port=%d, fd=%d, total=%d\n",
				get_current_time(), t_conn->host_addr,
				t_conn->host_port, t_conn->port, fd, serlink_count);

			if(((ext_conn_t *)(t_conn->extdata))->way == CONN_WITH_CLIENT)
			{
				if(((ext_conn_t *)(t_conn->extdata))->toconn == NULL)
				{
					AO_PRINTF("[%s] line:%d to target\n", get_current_time(), __LINE__);
					release_connection_with_fd(fd);
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

void time_handler()
{
	if(serlink_count < get_max_connections_num())
	{
		int i = get_max_connections_num() - serlink_count;
		while(i-- > 0)
		{
			try_connect(get_host_addr(), get_host_port(), CONN_WITH_SERVER);
		}
	}
	else if(serlink_count <= 10)
	{
		net_tcp_connect(get_host_addr(), get_host_port());
	}
}

void data_handler(int fd, char *data, int len)
{
	tcp_conn_t *tconn = queryfrom_tcpconn_list(fd);
	if(tconn)
	{
		if(((ext_conn_t *)(tconn->extdata))->toconn)
		{
			send(((ext_conn_t *)tconn->extdata)->toconn->fd, data, len, 0);
			((ext_conn_t *)tconn->extdata)->isuse = 1;
			if(((ext_conn_t *)(tconn->extdata))->way==CONN_WITH_SERVER)
			{
				AO_PRINTF("[%s] %s:%d ==> %s:%d, fd=%d\n", get_current_time(),
					tconn->host_addr, tconn->host_port,
					((ext_conn_t *)tconn->extdata)->toconn->host_addr,
					((ext_conn_t *)tconn->extdata)->toconn->host_port,
					tconn->fd);
			}
			else
			{
				AO_PRINTF("[%s] %s:%d <== %s:%d, fd=%d\n", get_current_time(),
					((ext_conn_t *)tconn->extdata)->toconn->host_addr,
					((ext_conn_t *)tconn->extdata)->toconn->host_port,
					tconn->host_addr, tconn->host_port, tconn->fd);
			}
		}
		else if(((ext_conn_t *)(tconn->extdata))->way == CONN_WITH_SERVER)
		{
			tcp_conn_t *cliconn = 
				try_connect("127.0.0.1", get_transport(), CONN_WITH_CLIENT);
			if(cliconn)
			{
				((ext_conn_t *)(tconn->extdata))->toconn = cliconn;
				((ext_conn_t *)(cliconn->extdata))->toconn = tconn;
				send(cliconn->fd, data, len, 0);
				((ext_conn_t *)tconn->extdata)->isuse = 1;
				AO_PRINTF("[%s] %s:%d ==> %s:%d, fd=%d\n",
					get_current_time(),
					tconn->host_addr,
					tconn->host_port,
					cliconn->host_addr,
					cliconn->host_port, 
					tconn->fd);
			}
		}
	}
	else
	{
		AO_PRINTF("[%s] can't find connection from list, fd=%d\n",
			get_current_time(), fd);
	}
}

