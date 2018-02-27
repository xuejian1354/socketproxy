/*
 * nethandler.c
 */

#include "nethandler.h"
#include "netlist.h"
#include <errno.h>


static int nbytes;
static char buf[MAXSIZE];
static char rate_print[64];

struct timespec *select_time = NULL;
struct timespec local_time;

int serlink_count = 0;

int before_channel;
int iswork = 0;

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

char *get_rate_print(uint32 rate)
{
	bzero(rate_print, sizeof(rate_print));
	if(rate < 1024)
	{
		sprintf(rate_print, "%u B/s", rate);
	}
	else if(rate < 1024*1024)
	{
		sprintf(rate_print, "%.1f KB/s", (float)(rate*10/1024)/10);
	}
	else if(rate < 1024*1024*1024)
	{
		sprintf(rate_print, "%.2f MB/s", (float)(rate*100/(1024*1024))/100);
	}
	else
	{
		sprintf(rate_print, "%u.2f G/s", (float)(rate*100/(1024*1024*1024))/100);
	}

	return rate_print;
}

void send_with_rate_callback(int fd, char *data, int len, 
	tcp_conn_t *src_conn, tcp_conn_t *dst_conn,
	void(*rate_call)(float, tcp_conn_t *, tcp_conn_t *))
{
	int st = 100;
	unsigned long sbtime = get_system_time();
	while(send(fd, data, len, 0) < 0)
	{
		if(errno == EAGAIN)
        {
            usleep(st);
			if(st < 1000) st += 100;
			else if(st < 50000) st += 1000;
			else st += 100000;

            //printf("send() again: errno==EAGAIN\n");
			continue;
        }
        else
        {
			char ebuf[128] = {0};
            sprintf(ebuf, "send() ERROR errno=%d, strerror=%s", errno, strerror(errno));
			perror(ebuf);
            return;
        }
	}

	rate_call(((float)len*1000)/(get_system_time()-sbtime), src_conn, dst_conn);
}

void close_connection(int fd)
{
	close(fd);
	select_clr(fd);
}

void close_connection_from_list(tcp_conn_t *tconn)
{
	if(tconn)
	{
		if(tconn->isconnect)
		{
			close(tconn->fd);
		}
		select_clr(tconn->fd);
	}
}

void detect_link()
{
	if(iswork && serlink_count <= 10)
	{
		//clear_all_conn(close_connection_from_list);
		set_timespec(get_rand(1200, 2400));
	}
	else if(serlink_count < get_max_connections_num())
	{
		set_timespec(get_rand(58, 118));
	}
	else
	{
		iswork = 1;
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

void send_to_stream_call(float rate, tcp_conn_t *src_conn, tcp_conn_t *dst_conn)
{
	((ext_conn_t *)src_conn->extdata)->isuse = 1;

	if(before_channel == src_conn->fd)
	{
		AT_PRINTF("\033[1A");
	}

	AT_PRINTF("[%s] %s:%d ==> %s:%d, fd=%d, %s\n",
		get_current_time(), src_conn->host_addr, src_conn->host_port,
		dst_conn->host_addr, dst_conn->host_port, src_conn->fd, get_rate_print(rate));

	before_channel = src_conn->fd;
}

void send_back_stream_call(float rate, tcp_conn_t *src_conn, tcp_conn_t *dst_conn)
{
	((ext_conn_t *)dst_conn->extdata)->isuse = 1;

	if(before_channel == src_conn->fd)
	{
		AT_PRINTF("\033[1A");
	}

	AT_PRINTF("[%s] %s:%d <== %s:%d, fd=%d, %s\n",
		get_current_time(), dst_conn->host_addr, dst_conn->host_port,
		src_conn->host_addr, src_conn->host_port, src_conn->fd, get_rate_print(rate));

	before_channel = src_conn->fd;
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
	   	if ((nbytes = recv(fd, buf, sizeof(buf), 0)) <= 0)
	   	{
			int isreconnect = 0;
			ext_conn_t *extdata = (ext_conn_t *)(t_conn->extdata);

			tcp_conn_t *toconn = extdata->toconn;
			if(toconn && toconn->isconnect)
			{	if(extdata->way == CONN_WITH_SERVER)
					AO_PRINTF("[%s] line:%d to target\n", get_current_time(), __LINE__);
				else
				{
					serlink_count--;
					if(((ext_conn_t *)(toconn->extdata))->isuse)
						isreconnect = 1;
					AO_PRINTF("[%s] line:%d to server\n", get_current_time(), __LINE__);
				}
				release_connection_with_fd(toconn->fd);
			}
			else
			{
				toconn = NULL;
			}

			if(extdata->way == CONN_WITH_SERVER)
			{
				serlink_count--;
				if(extdata->isuse)
					isreconnect = 1;

				AO_PRINTF("[%s] line:%d to server\n", get_current_time(), __LINE__);
			}
			else
				AO_PRINTF("[%s] line:%d to target\n", get_current_time(), __LINE__);

			release_connection_with_fd(fd);
			if(isreconnect)
			{
				try_connect(get_host_addr(), get_host_port(), CONN_WITH_SERVER);
			}
		}
		else
		{
			//PRINT_HEX(buf, nbytes);
			tcp_conn_t *toconn = ((ext_conn_t *)(t_conn->extdata))->toconn;
			if(toconn)
			{
				if(((ext_conn_t *)(t_conn->extdata))->way == CONN_WITH_SERVER)
				{
					send_with_rate_callback(toconn->fd, buf, nbytes, t_conn, toconn,
												send_to_stream_call);
				}
				else
				{
					send_with_rate_callback(toconn->fd, buf, nbytes, t_conn, toconn,
												send_back_stream_call);
				}
			}
			else if(((ext_conn_t *)(t_conn->extdata))->way == CONN_WITH_SERVER)
			{
				tcp_conn_t *cliconn = 
					try_connect("127.0.0.1", get_transport(), CONN_WITH_CLIENT);
				if(cliconn)
				{
					((ext_conn_t *)(t_conn->extdata))->toconn = cliconn;
					((ext_conn_t *)(cliconn->extdata))->toconn = t_conn;

					fcntl(cliconn->fd, F_SETFL,
						fcntl(cliconn->fd, F_GETFL, 0) | O_NONBLOCK);
					int tlen;
					char tbuf[MAXSIZE];
					while ((tlen = recv(cliconn->fd, tbuf, sizeof(tbuf), 0)) > 0)
					{
						AO_PRINTF("[%s] Ignore data from frame\n", get_current_time());
					}

					send_with_rate_callback(cliconn->fd, buf, nbytes, t_conn, cliconn,
												send_to_stream_call);
				}
				else
				{
					AO_PRINTF("[%s] connecting to client missing\n", get_current_time());
				}
			}
			else
			{
				AO_PRINTF("[%s] No object to transfer data\n", get_current_time());
			}
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
	AO_PRINTF("[%s] time handle, %d\n", get_current_time(), get_timespec()->tv_sec);
	if(serlink_count < get_max_connections_num())
	{
		int i = get_max_connections_num() - serlink_count;
		while(i-- > 0)
		{
			try_connect(get_host_addr(), get_host_port(), CONN_WITH_SERVER);
		}
	}
}

