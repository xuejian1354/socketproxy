/*
 * netapi.c
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

#include "netapi.h"
#include "netlist.h"

#define ADDR_MAXSIZE	128

static int c_tcpfd;

static int nbytes;
static char buf[MAXSIZE];
static char rebuf[MAXSIZE+136];


static enum RecvActionEU r_act = RECV_ACTION_START;
static uint8 r_cmd;
static uint8 r_addrlen;
static uint32 r_datalen;
static int r_count;
static uint8 addr_buf[ADDR_MAXSIZE];

static int gw_port;
static char user_ip[64];
static int user_port;
static char dst_ip[64];
static int dst_port;

void data_handler(int fd, char *data, int len);

int get_ctcp_fd()
{
	return c_tcpfd;
}

int try_dst_connect(char *host, int port)
{
	int fd;
	struct hostent *nlp_host;

	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("client tcp socket fail");
		return -1;
	}

	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags|O_NONBLOCK);

	while ((nlp_host=gethostbyname(host))==NULL){
    	perror("Resolve Error!\n");
		return -1;
	}

	struct sockaddr_in m_server_addr;
	m_server_addr.sin_family = PF_INET;
	m_server_addr.sin_port = htons(port);
	m_server_addr.sin_addr.s_addr = 
		((struct in_addr *)(nlp_host->h_addr))->s_addr;

	int res = connect(fd, (struct sockaddr *)&m_server_addr, sizeof(m_server_addr));
	if(0 != res & errno != EINPROGRESS)
	{
		perror("client tcp socket connect server fail");
		AO_PRINTF("[%s] connect %s fail\n", get_current_time(), host);
		return -1;
	}

	tcp_conn_t *tconn = calloc(1, sizeof(tcp_conn_t));
	tconn->fd = fd;
	tconn->isconnect = (0==res ? 1:0);
	tconn->port = user_port;
	strcpy(tconn->host, host);
	tconn->client_addr = m_server_addr;
	tconn->next = NULL;
	addto_tcpconn_list(tconn);

	select_wtset(fd);
	return fd;
}

int socket_tcp_client_connect(char *host, int port)
{
	c_tcpfd = try_dst_connect(host, port);
	return c_tcpfd;
}

int socket_tcp_client_recv(int fd)
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
			close(fd);
			delfrom_tcpconn_list(fd);
			select_clr(fd);
			AI_PRINTF("[%s] close, fd=%d\n", get_current_time(), fd);
			if(fd != get_ctcp_fd())
			{
				char *conn_close = RECV_PACK_DATA_CLOSE;
				int close_len = strlen(conn_close);

				user_port = t_conn->port;
				memset(dst_ip, 0, sizeof(dst_ip));
				sprintf(dst_ip, "%s", t_conn->host);
				dst_port = ntohs(t_conn->client_addr.sin_port);

				memset(addr_buf, 0, sizeof(addr_buf));
				sprintf(addr_buf, "%d|%s|%d|%s|%d",
					gw_port, user_ip, user_port, dst_ip, dst_port);

				rebuf[0] = RECV_PACK_START;
				rebuf[1] = RECV_CMD_CLOSE;
				rebuf[2] = strlen(addr_buf);
				rebuf[6] = close_len & 0xFF;
				rebuf[5] = (close_len>>8) & 0xFF;
				rebuf[4] = (close_len>>16) & 0xFF;
				rebuf[3] = (close_len>>24) & 0xFF;
				strcpy(rebuf+7, addr_buf);
				memcpy(rebuf+7+rebuf[2], conn_close, close_len);

				send(get_ctcp_fd(), rebuf, 7+rebuf[2]+close_len, 0);
			}
			else
			{
				AO_PRINTF("[%s] Main tcp connection closed, fd=%d\n",
					get_current_time(), fd);
				set_end(0);
				return -1;
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

		user_port = t_conn->port;
		memset(dst_ip, 0, sizeof(dst_ip));
		sprintf(dst_ip, "%s", t_conn->host);
		dst_port = ntohs(t_conn->client_addr.sin_port);

        if (0 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &errinfo, &errlen))    
        {
			t_conn->isconnect = 1;
			select_set(fd);
			AO_PRINTF("[%s] connect %s:%d, fd=%d\n", get_current_time(), dst_ip, dst_port, fd);
        }
		else
		{
			AO_PRINTF("[%s] connect %s fail\n", get_current_time(), dst_ip, fd);
		}

		select_wtclr(fd);

		if(fd != get_ctcp_fd())
		{
			if(t_conn->isconnect)
			{
				char *open_success = RECV_PACK_DATA_OPEN_SUCCESS;
				int success_len = strlen(open_success);

				memset(addr_buf, 0, sizeof(addr_buf));
				sprintf(addr_buf, "%d|%s|%d|%s|%d",
					gw_port, user_ip, user_port, dst_ip, dst_port);

				rebuf[0] = RECV_PACK_START;
				rebuf[1] = RECV_CMD_OPEN;
				rebuf[2] = strlen(addr_buf);
				rebuf[6] = success_len & 0xFF;
				rebuf[5] = (success_len>>8) & 0xFF;
				rebuf[4] = (success_len>>16) & 0xFF;
				rebuf[3] = (success_len>>24) & 0xFF;
				strcpy(rebuf+7, addr_buf);
				memcpy(rebuf+7+rebuf[2], open_success, success_len);

				send(get_ctcp_fd(), rebuf, 7+rebuf[2]+success_len, 0);
			}
			else
			{
				char *open_fail = RECV_PACK_DATA_OPEN_FAIL;
				int fail_len = strlen(open_fail);

				memset(addr_buf, 0, sizeof(addr_buf));
				sprintf(addr_buf, "%d|%s|%d|%s|%d",
					gw_port, user_ip, user_port, dst_ip, dst_port);

				rebuf[0] = RECV_PACK_START;
				rebuf[1] = RECV_CMD_OPEN;
				rebuf[2] = strlen(addr_buf);
				rebuf[6] = fail_len & 0xFF;
				rebuf[5] = (fail_len>>8) & 0xFF;
				rebuf[4] = (fail_len>>16) & 0xFF;
				rebuf[3] = (fail_len>>24) & 0xFF;
				strcpy(rebuf+7, addr_buf);
				memcpy(rebuf+7+rebuf[2], open_fail, fail_len);

				send(get_ctcp_fd(), rebuf, 7+rebuf[2]+fail_len, 0);
			}
		}
		
	}

	return 0;
}

void data_handler(int fd, char *data, int len)
{
	if(fd == get_ctcp_fd())	//server to dst
	{
		int i = 0;
		int bnext_start_index = -1;
		while(i < len)
		{
			if(r_act != RECV_ACTION_START
				&& *(data+i) == RECV_PACK_START
				&& bnext_start_index < 0)
			{
				bnext_start_index = i;
			}

			switch(r_act)
			{
			case RECV_ACTION_START:
				if(bnext_start_index >= 0)
				{
					i = bnext_start_index;
					bnext_start_index = -1;
					r_act = RECV_ACTION_CMD;
					break;
				}

				if(*(data+i) == RECV_PACK_START)
				{
					r_act = RECV_ACTION_CMD;
				}
				break;

			case RECV_ACTION_CMD:
				r_cmd = *(data+i);
				r_act = RECV_ACTION_ADDRLEN;
				break;

			case RECV_ACTION_ADDRLEN:
				r_addrlen = *(data+i);
				if(r_addrlen > ADDR_MAXSIZE)
				{
					r_act = RECV_ACTION_START;
					break;
				}
				r_datalen = 0;
				r_count = 0;
				r_act = RECV_ACTION_DATALEN;
				break;

			case RECV_ACTION_DATALEN:
				r_datalen = (r_datalen<<8) + *(data+i);
				r_count++;
				if(r_count == 4)
				{
					r_count = 0;
					r_act = RECV_ACTION_ADDR;
				}
				break;

			case RECV_ACTION_ADDR:
				if(*(data+i) == '|')
				{
					addr_buf[r_count++]	= '\0';
				}
				else
				{
					addr_buf[r_count++] = *(data+i);
				}

				if(r_count == r_addrlen) {
					addr_buf[r_count] = '\0';

					int y = 0;
					int x = 0;
					int xblank_pos[4];
					while(y++ < r_addrlen-1 && x < 4)
					{
						if(addr_buf[y] == '\0')
						{
							xblank_pos[x++] = y;
						}
					}

					if(x == 4)
					{
						gw_port = atoi(addr_buf);
						strcpy(user_ip, addr_buf+xblank_pos[0]+1);
						user_port= atoi(addr_buf+xblank_pos[1]+1);
						strcpy(dst_ip, addr_buf+xblank_pos[2]+1);
						dst_port = atoi(addr_buf+xblank_pos[3]+1);
					}
					else
					{
						r_act = RECV_ACTION_START;
						break;
					}

					r_count = 0;
					r_act = RECV_ACTION_DATA;
				}
				break;

			case RECV_ACTION_DATA:
				switch(r_cmd) {
				case RECV_CMD_OPEN:
					if(try_dst_connect(dst_ip, dst_port) < 0)
					{
						char *open_fail = RECV_PACK_DATA_OPEN_FAIL;
						int fail_len = strlen(open_fail);

						memset(addr_buf, 0, sizeof(addr_buf));
						sprintf(addr_buf, "%d|%s|%d|%s|%d",
							gw_port, user_ip, user_port, dst_ip, dst_port);

						rebuf[0] = RECV_PACK_START;
						rebuf[1] = RECV_CMD_OPEN;
						rebuf[2] = strlen(addr_buf);
						rebuf[6] = fail_len & 0xFF;
						rebuf[5] = (fail_len>>8) & 0xFF;
						rebuf[4] = (fail_len>>16) & 0xFF;
						rebuf[3] = (fail_len>>24) & 0xFF;
						strcpy(rebuf+7, addr_buf);
						memcpy(rebuf+7+rebuf[2], open_fail, fail_len);

						if(fd != get_ctcp_fd())
							send(get_ctcp_fd(), rebuf, 7+rebuf[2]+fail_len, 0);
					}
					bnext_start_index = -1;
					r_act = RECV_ACTION_START;
					return;

				case RECV_CMD_MESSAGE:
				{
					printf("[%s] ==> %s:%u\n", get_current_time(), dst_ip, dst_port);
					tcp_conn_t *t_list =
						queryfrom_tcpconn_list_with_localport(user_port);
					if(t_list != NULL)
					{
						int datalen = len - i;
						if(datalen < r_datalen)
						{
							send(t_list->fd, data+i, datalen, 0);
						}
						else
						{
							send(t_list->fd, data+i, r_datalen, 0);
						}
					}
				}
					break;

				case RECV_CMD_CLOSE:
				{
					tcp_conn_t *t_list =
						queryfrom_tcpconn_list_with_localport(user_port);
					if(t_list != NULL)
					{
						t_list->isconnect = 0;
						close(t_list->fd);
						delfrom_tcpconn_list(fd);
						select_clr(fd);
						AI_PRINTF("[%s] cmd close, fd=%d\n", get_current_time(), fd);
					}

					char *conn_close = RECV_PACK_DATA_CLOSE;
					int close_len = strlen(conn_close);

					memset(addr_buf, 0, sizeof(addr_buf));
					sprintf(addr_buf, "%d|%s|%d|%s|%d",
						gw_port, user_ip, user_port, dst_ip, dst_port);

					rebuf[0] = RECV_PACK_START;
					rebuf[1] = RECV_CMD_CLOSE;
					rebuf[2] = strlen(addr_buf);
					rebuf[6] = close_len & 0xFF;
					rebuf[5] = (close_len>>8) & 0xFF;
					rebuf[3] = (close_len>>16) & 0xFF;
					rebuf[3] = (close_len>>24) & 0xFF;
					strcpy(rebuf+7, addr_buf);
					memcpy(rebuf+7+rebuf[2], conn_close, close_len);

					if(fd != get_ctcp_fd())
						send(get_ctcp_fd(), rebuf, 7+rebuf[2]+close_len, 0);

					bnext_start_index = -1;
					r_act = RECV_ACTION_START;
				}
					return;

				case RECV_CMD_MAC:
				case RECV_CMD_HEART:
					r_act = RECV_ACTION_START;
					return;
				}

				if(i + r_datalen <= len)
				{
					i += r_datalen-1;
					bnext_start_index = -1;
					r_act = RECV_ACTION_START;
				}
				else
				{
					i = len-1;
					r_datalen -= len - i;
				}
				break;
			}

			i++;
		}
		r_act = RECV_ACTION_START;
	}
	else	// dst back to serve
	{
		tcp_conn_t *t_list = queryfrom_tcpconn_list(fd);
		if(t_list != NULL)
		{
			user_port = t_list->port;
			memset(dst_ip, 0, sizeof(dst_ip));
			sprintf(dst_ip, "%s", t_list->host);
			dst_port = ntohs(t_list->client_addr.sin_port);

			memset(addr_buf, 0, sizeof(addr_buf));
			sprintf(addr_buf, "%d|%s|%d|%s|%d",
				gw_port, user_ip, user_port, dst_ip, dst_port);

			rebuf[0] = RECV_PACK_START;
			rebuf[1] = RECV_CMD_MESSAGE;
			rebuf[2] = strlen(addr_buf);
			rebuf[6] = len & 0xFF;
			rebuf[5] = (len>>8) & 0xFF;
			rebuf[4] = (len>>16) & 0xFF;
			rebuf[3] = (len>>24) & 0xFF;
			strcpy(rebuf+7, addr_buf);
			memcpy(rebuf+7+rebuf[2], data, len);

			send(get_ctcp_fd(), rebuf, 7+rebuf[2]+len, 0);
		}
	}
}

