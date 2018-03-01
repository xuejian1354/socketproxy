#include "globals.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

extern char auth_user[128];
extern char auth_pass[128];

int lcfd, lgfd;
int client_port, gateway_port;

int main(int argc, char **argv)
{
	int ch;
	int isget = 0;
	opterr = 0;

	const char *optstrs = "a:c:g:h";
    while((ch = getopt(argc, argv, optstrs)) != -1)
    {
		switch(ch)
		{
		case 'h':
#ifdef GWLINK_WITH_SOCKS5_PASS
			AI_PRINTF("Usage: %s -c <client port> -g <gateway port> [-a user:pass]\n", argv[0]);
			AI_PRINTF("Default\n");
			AI_PRINTF("    user:pass   %s:%s\n", DEFAULT_USER, DEFAULT_PASS);
#else
			AI_PRINTF("Usage: %s -c <client port> -g <gateway port>\n", argv[0]);
#endif
			return 1;
#ifdef GWLINK_WITH_SOCKS5_PASS
		case 'a':
			isget++;
			{
				int oalen = strlen(optarg);
				int oapos = 0;
				while(oapos < oalen)
				{
					if(*(optarg+oapos) == ':')
					{
						memset(auth_user, 0, sizeof(auth_user));
						memcpy(auth_user, optarg, oapos);
						memset(auth_pass, 0, sizeof(auth_pass));
						memcpy(auth_pass, optarg+oapos+1, oalen-oapos);
					}
					oapos++;
				}
			}
			break;
#endif
		case 'c':
			isget++;
			client_port = atoi(optarg);
			break;

		case 'g':
			isget++;
			gateway_port = atoi(optarg);
			break;
		}
	}

	if(isget < 2)
	{
		AI_PRINTF("Unrecognize arguments.\n");
		AI_PRINTF("\'%s -h\' get more help infomations.\n", argv[0]);
		return -1;
	}

	process_signal_register();

	AI_PRINTF("[%s] %s start, getpid=%d\n", get_current_time(), argv[0], getpid());

	int opt = 1;
	struct sockaddr_in scin, sgin;
	bzero(&scin, sizeof(struct sockaddr_in));
	scin.sin_family=AF_INET;
	scin.sin_addr.s_addr=htonl(INADDR_ANY);
	scin.sin_port=htons(client_port);

	bzero(&sgin,sizeof(struct sockaddr_in));
	sgin.sin_family=AF_INET;
	sgin.sin_addr.s_addr=htonl(INADDR_ANY);
	sgin.sin_port=htons(gateway_port);

	if((lcfd=socket(AF_INET, SOCK_STREAM, 0))==-1
		|| (lgfd=socket(AF_INET, SOCK_STREAM, 0))==-1)
	{
		 fprintf(stderr, "Socket error:%s\n\a", strerror(errno));
		 return -1;
	}

	//setsockopt(lcfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	//setsockopt(lgfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if(bind(lcfd,(struct sockaddr *)(&scin),sizeof(struct sockaddr))==-1
		|| bind(lgfd,(struct sockaddr *)(&sgin),sizeof(struct sockaddr))==-1)
	{
		fprintf(stderr,"Bind error:%s\n\a",strerror(errno));
		return -1;
	}

	if(listen(lcfd, 5)==-1 || listen(lgfd, 5)==-1)
	{
		fprintf(stderr,"Listen error:%s\n\a",strerror(errno));
		return -1;
	}

	printf("Accepting connections, listen client %d, gw %d .......\n",
			client_port, gateway_port);

	if (select_init() < 0)
	{
		return 1;
	}

	select_set(lcfd);
	select_set(lgfd);

	while(get_end())
	{
		if(select_listen(0) < 0)
		{
			set_end(0);
		}
		usleep(100);
	}

	AI_PRINTF("[%s] %s exit, %d\n", get_current_time(), argv[0], getpid());
	return 0;
}

int net_tcp_recv(int fd)
{
	if(fd == lcfd)
	{
		int cfd;
		struct sockaddr_in cin;
		int addr_len = sizeof(cin);
		if((cfd=accept(lcfd, (struct sockaddr *)(&cin), &addr_len))==-1)
		{
			fprintf(stderr,"Accept error:%s\n\a",strerror(errno));
			return;
		}
		printf("New client connection, fd=%d, %s:%u\n",
					cfd, inet_ntoa(cin.sin_addr), ntohs(cin.sin_port));

		ext_conn_t *extdata = calloc(1, sizeof(ext_conn_t));
		extdata->toconn = NULL;
		extdata->way = CONN_WITH_CLIENT;
		extdata->isuse = 0;
		tcp_conn_t *tconn =
			new_tcpconn(cfd, GWLINK_START, cin.sin_port, "0.0.0.0", client_port, extdata);
		if(tconn == NULL)
		{
			free(extdata);
			return 1;
		}

		addto_tcpconn_list(tconn);
		select_set(cfd);
	}
	else if(fd == lgfd)
	{
		int gfd;
		struct sockaddr_in cin;
		int addr_len = sizeof(cin);
		if((gfd=accept(lgfd, (struct sockaddr *)(&cin), &addr_len))==-1)
		{
			fprintf(stderr,"Accept error:%s\n\a",strerror(errno));
			return;
		}
		printf("New gateway connection, fd=%d\n", gfd);

		ext_conn_t *extdata = calloc(1, sizeof(ext_conn_t));
		extdata->toconn = NULL;
		extdata->way = CONN_WITH_SERVER;	//means gateway
		extdata->isuse = 0;
#ifdef GWLINK_WITH_SOCKS5_PASS
		tcp_conn_t *tconn =
			new_tcpconn(gfd, GWLINK_AUTH, cin.sin_port, "0.0.0.0", gateway_port, extdata);
#else
		tcp_conn_t *tconn =
			new_tcpconn(gfd, GWLINK_START, cin.sin_port, "0.0.0.0", gateway_port, extdata);
#endif
		if(tconn == NULL)
		{
			free(extdata);
			return 1;
		}

		addto_tcpconn_list(tconn);
		select_set(gfd);
	}
	else
	{
		tcp_conn_t *tconn = queryfrom_tcpconn_list(fd);
		if(tconn)
		{
			ext_conn_t *extdata = tconn->extdata;
			if(extdata->way == CONN_WITH_CLIENT)
			{
				if(!extdata->toconn)
				{
					tcp_conn_list_t *p_list = get_tcp_conn_list();
					tcp_conn_t *t_list;
					if(p_list->p_head != NULL)
					{
						ext_conn_t *todata;
						for(t_list=p_list->p_head; t_list!=NULL; t_list=t_list->next)
						{
							todata = t_list->extdata;
							if(todata->way == CONN_WITH_SERVER
								&& !todata->isuse
								&& t_list->gwlink_status == GWLINK_START)
							{
								extdata->toconn = t_list;
								todata->toconn = tconn;
								todata->isuse = 1;
								break;
							}
						}
					}
				}
			}

			int dlen;
			char dbuf[MAXSIZE];
			memset(dbuf, 0, sizeof(dbuf));
		   	if ((dlen = recv(fd, dbuf, sizeof(dbuf), 0)) <= 0)
		   	{
				if(extdata->toconn)
				{
					int tofd = extdata->toconn->fd;
					close(tofd);
					select_clr(0, tofd);
					delfrom_tcpconn_list(tofd);
					printf("%d: close, fd=%d\n", __LINE__, tofd);
				}

				close(fd);
				select_clr(0, fd);
				delfrom_tcpconn_list(fd);
				printf("%d: close, fd=%d\n", __LINE__, fd);
				return 0;
			}
#ifdef GWLINK_WITH_SOCKS5_PASS
			if(extdata->way == CONN_WITH_SERVER)
			{
				switch(tconn->gwlink_status)
				{
				case GWLINK_AUTH:
					if(dlen > 0 && dbuf[0] == 0x5)
					{
						if(dlen > 2)
						{
							int i = 2;
							while(i < dbuf[1]+2)
							{
								if(dbuf[i] == 0x2)
								{
									char rebuf[2] = {0x5, 0x2};
									if(send(fd, rebuf, 2, 0) > 0)
									{
										tconn->gwlink_status = GWLINK_PASS;
									}
									break;
								}
								i++;
							}
						}
					}
					return 0;

				case GWLINK_PASS:
					//PRINT_HEX(dbuf, dlen);
					if(dlen > 0 && dbuf[0] == 0x1)
					{
						if(dlen > 1 && dbuf[1]+3<dlen && 3+dbuf[1]+dbuf[2+dbuf[1]]<=dlen)
						{
							char user[128] = {0};
							char pass[128] = {0};
							memcpy(user, dbuf+2, dbuf[1]);
							memcpy(pass, dbuf+3+dbuf[1], dbuf[2+dbuf[1]]);
							if(!strcmp(user, get_auth_user())
								&& strlen(user) == strlen(get_auth_user())
								&& !strcmp(pass, get_auth_pass())
								&& strlen(pass) == strlen(get_auth_pass()))
							{
								char rebuf[2] = {0x1, 0};
								if(send(fd, rebuf, 2, 0) > 0)
								{
									tconn->gwlink_status = GWLINK_START;
								}
								return 0;
							}
							else
							{
								char rebuf[2] = {0x1, 0x1};
								send(fd, rebuf, 2, 0);
							}
						}
					}

					close(fd);
					select_clr(0, fd);
					delfrom_tcpconn_list(fd);
					printf("%d: close, fd=%d\n", __LINE__, fd);
					return 0;

				default:
					break;
				}
			}
#endif
			if(extdata->toconn)
			{
				if(send(extdata->toconn->fd, dbuf, dlen, 0) < 0)
				{
					perror("send()");

					int tofd = extdata->toconn->fd;
					close(tofd);
					select_clr(0, tofd);
					delfrom_tcpconn_list(tofd);
					printf("%d: close, fd=%d\n", __LINE__, tofd);

					close(fd);
					select_clr(0, fd);
					delfrom_tcpconn_list(fd);
					printf("%d: close, fd=%d\n", __LINE__, fd);
				}
			}
			else
			{
				close(fd);
				select_clr(0, fd);
				delfrom_tcpconn_list(fd);
				printf("%d: close, fd=%d\n", __LINE__, fd);
			}
		}
	}

	return 0;
}

void time_handler(int index)
{
}

struct timespec *get_timespec()
{
	return NULL;
}

#ifdef __cplusplus
}
#endif

