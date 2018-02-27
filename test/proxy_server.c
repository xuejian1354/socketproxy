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

int lcfd, lgfd;
int client_port, gateway_port;

int main(int argc, char **argv)
{
	int ch;
	int isget = 0;
	opterr = 0;

	const char *optstrs = "c:g:h";
    while((ch = getopt(argc, argv, optstrs)) != -1)
    {
		switch(ch)
		{
		case 'h':
			AI_PRINTF("Usage: %s -c <client port> -g <gateway port>\n", argv[0]);
			return 1;


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

	setsockopt(lcfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	setsockopt(lgfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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
		if(select_listen() < 0)
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
			new_tcpconn(cfd, 1, cin.sin_port, "0.0.0.0", client_port, extdata);
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
		tcp_conn_t *tconn =
			new_tcpconn(gfd, 1, cin.sin_port, "0.0.0.0", gateway_port, extdata);
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
							if(todata->way == CONN_WITH_SERVER && !todata->isuse)
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
					select_clr(tofd);
					delfrom_tcpconn_list(tofd);
					printf("%d: close, fd=%d\n", __LINE__, tofd);
				}

				close(fd);
				select_clr(fd);
				delfrom_tcpconn_list(fd);
				printf("%d: close, fd=%d\n", __LINE__, fd);
				return 0;
			}

			if(extdata->toconn)
			{
				if(send(extdata->toconn->fd, dbuf, dlen, 0) < 0)
				{
					perror("send()");

					int tofd = extdata->toconn->fd;
					close(tofd);
					select_clr(tofd);
					delfrom_tcpconn_list(tofd);
					printf("%d: close, fd=%d\n", __LINE__, tofd);

					close(fd);
					select_clr(fd);
					delfrom_tcpconn_list(fd);
					printf("%d: close, fd=%d\n", __LINE__, fd);
				}
			}
			else
			{
				close(fd);
				select_clr(fd);
				delfrom_tcpconn_list(fd);
				printf("%d: close, fd=%d\n", __LINE__, fd);
			}
		}
	}

	return 0;
}

void time_handler()
{
}

struct timespec *get_timespec()
{
	return NULL;
}

#ifdef __cplusplus
}
#endif

