/*
 * nethandler.c
 */

#include "nethandler.h"
#include "netlist.h"
#include <errno.h>

#ifdef GWLINK_WITH_SOCKS5_PASS
const char socks5_frame[] = {0x05, 0x01, 0x02};
char userpass_frame[256];
#endif

static int nbytes;
static char buf[MAXSIZE];
static char rate_print[64];

struct timespec *select_time = NULL;
struct timespec local_time;

int serlink_count[PTHREAD_SELECT_NUM] = {0};

int before_channel;
int iswork = 0;

int get_rand(int min, int max) {
	return (rand() % (max-min+1)) + min;
}

int get_total_serlink_count()
{
	int i;
	int total = 0;
	for(i=0; i<PTHREAD_SELECT_NUM; i++)
	{
		total += serlink_count[i];
	}

	return total;
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
	int stm = 100;
	unsigned long sbtime = get_system_time();
	while(send(fd, data, len, 0) < 0)
	{
		if(errno == EAGAIN)
        {
            usleep(stm);
			if(stm < 1000) stm += 100;
			else if(stm < 50000) stm += 1000;
			else stm += 100000;

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

void close_connection(int index, int fd)
{
	close(fd);
	select_clr(index, fd);
}

void detect_link(int index)
{
	if(iswork && serlink_count[index] <= 5)
	{
		set_timespec(get_rand(1200, 2400));
	}
	else if(serlink_count[index] < (get_max_connections_num()/PTHREAD_SELECT_NUM))
	{
		set_timespec(get_rand(58, 118));
	}
	else
	{
		iswork = 1;
		set_timespec(0);
	}
}

void release_connection_with_fd(int index, int fd)
{
	detect_link(index);
	close_connection(index, fd);
	delfrom_tcpconn_list(fd);
	AO_PRINTF("[%s] close, fd=%d, total=%d\n",
		get_current_time(), fd, get_total_serlink_count());
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

tcp_conn_t *try_connect(int index, char *host, int port, enum ConnWay way)
{
	int fd;
	tcp_conn_t *tconn;
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
	if(way != CONN_WITH_CLIENT)
	{
		tconn = new_tcpconn(fd, GWLINK_INIT, 0, host, port, extdata);
	}
	else
	{
		tconn = new_tcpconn(fd, GWLINK_START, 0, host, port, extdata);
	}

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

	if(way != CONN_WITH_CLIENT)
	{
		if(index < 0)
			tconn->pt_pos = select_wtset(fd);
		else
			tconn->pt_pos = select_wtset_with_index(index, fd);
	}
	else
	{
		if(index < 0)
			tconn->pt_pos = select_set(fd);
		else
			tconn->pt_pos = select_set_with_index(index, fd);
	}

	if(res == 0)
	{
		tconn->port = get_socket_local_port(fd);
		if(way != CONN_WITH_CLIENT)
		{
			serlink_count[tconn->pt_pos]++;
			detect_link(tconn->pt_pos);
#ifdef GWLINK_WITH_SOCKS5_PASS
			tconn->gwlink_status = GWLINK_AUTH;
#else
			tconn->gwlink_status = GWLINK_START;
#endif
		}
		else
		{
			tconn->gwlink_status = GWLINK_START;
		}

		AO_PRINTF("[%s] line %d connect %s:%d, current port=%d, fd=%d, total=%d\n",
				get_current_time(), __LINE__, tconn->host_addr,
				tconn->host_port, tconn->port, fd, get_total_serlink_count());
	}

	addto_tcpconn_list(tconn);
	return tconn;
}


int net_tcp_connect(char *host, int port)
{
	int i;
	for(i=0; i<get_max_connections_num(); i++)
	{
		try_connect(-1, get_host_addr(), get_host_port(), CONN_WITH_SERVER);
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

	if(t_conn->gwlink_status > GWLINK_INIT)
	{
	   	while ((nbytes = recv(fd, buf, sizeof(buf), 0)) <= 0)
	   	{
			int stm = 100;
			if(errno == EAGAIN)
			{
				usleep(stm);
				if(stm < 1000) stm += 100;
				else if(stm < 50000) stm += 1000;
				else stm += 100000;

				//printf("send() again: errno==EAGAIN\n");
				continue;
			}

			int isreconnect = 0;
			ext_conn_t *extdata = (ext_conn_t *)(t_conn->extdata);

			tcp_conn_t *toconn = extdata->toconn;
			if(toconn && toconn->gwlink_status > GWLINK_INIT)
			{	if(extdata->way == CONN_WITH_SERVER)
					AO_PRINTF("[%s] line:%d to target\n", get_current_time(), __LINE__);
				else
				{
					serlink_count[t_conn->pt_pos]--;
					if(((ext_conn_t *)(toconn->extdata))->isuse)
						isreconnect = 1;
					AO_PRINTF("[%s] line:%d to server\n", get_current_time(), __LINE__);
				}
				release_connection_with_fd(toconn->pt_pos, toconn->fd);
			}
			else
			{
				toconn = NULL;
			}

			if(extdata->way == CONN_WITH_SERVER)
			{
				serlink_count[t_conn->pt_pos]--;
				if(extdata->isuse)
					isreconnect = 1;

				AO_PRINTF("[%s] line:%d to server\n", get_current_time(), __LINE__);
			}
			else
				AO_PRINTF("[%s] line:%d to target\n", get_current_time(), __LINE__);

			int tpos = t_conn->pt_pos;
			release_connection_with_fd(t_conn->pt_pos, fd);
			if(isreconnect)
			{
				try_connect(tpos, get_host_addr(), get_host_port(), CONN_WITH_SERVER);
			}

			return 0;
		}

		if(1)
		{
			//PRINT_HEX(buf, nbytes);
#ifdef GWLINK_WITH_SOCKS5_PASS
			if(t_conn->gwlink_status == GWLINK_AUTH)
			{
				if(nbytes >= 2 && buf[0] == 0x05 && buf[1] == 0x02)
				{
					memset(userpass_frame, 0, sizeof(userpass_frame));
					userpass_frame[0] = 0x1;
					userpass_frame[1] = strlen(get_auth_user());
					memcpy(userpass_frame+2, get_auth_user(), userpass_frame[1]);
					userpass_frame[2+userpass_frame[1]] = strlen(get_auth_pass());
					memcpy(userpass_frame+3+userpass_frame[1],
						get_auth_pass(), userpass_frame[2+userpass_frame[1]]);

					while(send(fd, userpass_frame, strlen(userpass_frame), 0) < 0)
					{
						char ebuf[8] = {0};
			            sprintf(ebuf, "%d", __LINE__+1);
						perror(ebuf);

						if(errno == EAGAIN)
				        {
				            usleep(100000);
							continue;
				        }
				        else
				        {
				            return 0;;
				        }
					}

					t_conn->gwlink_status = GWLINK_PASS;
				}

				return 0;
			}
			else if(t_conn->gwlink_status == GWLINK_PASS)
			{
				if(nbytes >= 2 && buf[0] == 0x01 && buf[1] == 0)
				{
					t_conn->gwlink_status = GWLINK_START;
					AO_PRINTF("[%s] line %d: auth success, fd=%d\n", get_current_time(), __LINE__, t_conn->fd);
				}
				else if(nbytes >= 2 && buf[0] == 0x01 && buf[1] == 0x01)
				{
					t_conn->gwlink_status = GWLINK_AUTH;
					AO_PRINTF("[%s] line %d: auth fail, fd=%d\n", get_current_time(), __LINE__, t_conn->fd);
				}

				if(nbytes > 2)
				{
					char tbuf[nbytes-2];
					memcpy(tbuf, buf+2, nbytes-2);
					memcpy(buf, tbuf, nbytes-2);
					nbytes = nbytes - 2;
				}
				else
				{
					return 0;
				}
			}
#endif
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
					try_connect(t_conn->pt_pos, "127.0.0.1", get_transport(), CONN_WITH_CLIENT);
				if(cliconn)
				{
					((ext_conn_t *)(t_conn->extdata))->toconn = cliconn;
					((ext_conn_t *)(cliconn->extdata))->toconn = t_conn;

					/*fcntl(cliconn->fd, F_SETFL,
						fcntl(cliconn->fd, F_GETFL, 0) | O_NONBLOCK);
					int tlen;
					char tbuf[MAXSIZE];
					while ((tlen = recv(cliconn->fd, tbuf, sizeof(tbuf), 0)) > 0)
					{
						AO_PRINTF("[%s] Ignore data from frame\n", get_current_time());
					}*/

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
		int pt_pos = t_conn->pt_pos;
		errlen = sizeof(errlen);

        if (0 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &errinfo, &errlen))    
        {
			fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
			t_conn->port = get_socket_local_port(fd);
			if(t_conn->pt_pos < 0)
			{
				t_conn->pt_pos = select_set(fd);
				pt_pos = t_conn->pt_pos;
			}
			else
			{
				select_set_with_index(t_conn->pt_pos, fd);
			}

			if(((ext_conn_t *)(t_conn->extdata))->way == CONN_WITH_SERVER)
			{
#ifdef GWLINK_WITH_SOCKS5_PASS
				t_conn->gwlink_status = GWLINK_AUTH;

				while(send(fd, socks5_frame, sizeof(socks5_frame), 0) < 0)
				{
					char ebuf[8] = {0};
		            sprintf(ebuf, "%d", __LINE__+1);
					perror(ebuf);

					if(errno == EAGAIN)
			        {
			            usleep(100000);
						continue;
			        }
			        else
			        {
						t_conn->gwlink_status = GWLINK_START;
			            break;
			        }
				}
#else
				t_conn->gwlink_status = GWLINK_START;
#endif
				serlink_count[t_conn->pt_pos]++;
				detect_link(t_conn->pt_pos);
			}

			AO_PRINTF("[%s] line %d connect %s:%d, current port=%d, fd=%d, total=%d\n",
				get_current_time(), __LINE__, t_conn->host_addr,
				t_conn->host_port, t_conn->port, fd, get_total_serlink_count());

			if(((ext_conn_t *)(t_conn->extdata))->way == CONN_WITH_CLIENT)
			{
				t_conn->gwlink_status = GWLINK_START;
				if(((ext_conn_t *)(t_conn->extdata))->toconn == NULL)
				{
					AO_PRINTF("[%s] line:%d to target\n", get_current_time(), __LINE__);
					release_connection_with_fd(t_conn->pt_pos, fd);
				}
			}
        }
		else
		{
			AO_PRINTF("[%s] connect %s fail\n",
				get_current_time(), t_conn->host_addr, fd);
		}

		select_wtclr(pt_pos, fd);
	}

	return 0;
}

void time_handler(int index)
{
	AO_PRINTF("[%s] time handle, %d\n", get_current_time(), get_timespec()->tv_sec);
	if(serlink_count[index] < (get_max_connections_num()/PTHREAD_SELECT_NUM))
	{
		int i = (get_max_connections_num()/PTHREAD_SELECT_NUM) - serlink_count[index];
		while(i-- > 0)
		{
			try_connect(index, get_host_addr(), get_host_port(), CONN_WITH_SERVER);
		}
	}
}

