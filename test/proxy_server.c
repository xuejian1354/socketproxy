#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

#define MAX_LINE 1024

void main(int argc, char **argv)
{
	int  lcfd, lgfd;
	int cfd;
	int sfd;
	int rdy;

	struct sockaddr_in scin, sgin;
	struct sockaddr_in cin;

	int client[FD_SETSIZE];
	int gateway[FD_SETSIZE];

	int maxi;
	int maxfd;

	fd_set rset;
	fd_set allset;
	socklen_t addr_len;

	char buffer[MAX_LINE];
	int i;
	int n;
	int len;
	int opt = 1;
	char addr_p[20];

	int cli_fi = 0;
	int gw_fi = 0;

	int client_port = 8180;
	int gw_port = 8181;
	if(argc > 2)
	{
		client_port = atoi(argv[1]);
		gw_port = atoi(argv[2]);
	}

	bzero(&scin,sizeof(struct sockaddr_in));
	scin.sin_family=AF_INET;
	scin.sin_addr.s_addr=htonl(INADDR_ANY);
	scin.sin_port=htons(client_port);

	bzero(&sgin,sizeof(struct sockaddr_in));
	sgin.sin_family=AF_INET;
	sgin.sin_addr.s_addr=htonl(INADDR_ANY);
	sgin.sin_port=htons(gw_port);

	if((lcfd=socket(AF_INET,SOCK_STREAM,0))==-1
		|| (lgfd=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		fprintf(stderr,"Socket error:%s\n\a",strerror(errno));
		return;
	}

	setsockopt(lcfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	setsockopt(lgfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if(bind(lcfd,(struct sockaddr *)(&scin),sizeof(struct sockaddr))==-1
		|| bind(lgfd,(struct sockaddr *)(&sgin),sizeof(struct sockaddr))==-1)
	{
		fprintf(stderr,"Bind error:%s\n\a",strerror(errno));
		return;
	}

	if(listen(lcfd,20)==-1 || listen(lgfd,20)==-1)
	{
		fprintf(stderr,"Listen error:%s\n\a",strerror(errno));
		return;
	}

	printf("Accepting connections, listen client %d, gw %d .......\n",
		client_port, gw_port);

	maxfd = lcfd>lgfd?lcfd:lgfd;
	maxi = -1;

	for(i = 0;i < FD_SETSIZE;i++)
	{
		client[i] = -1;
		gateway[i] = -1;
	}

	FD_ZERO(&allset);
	FD_SET(lcfd,&allset);
	FD_SET(lgfd,&allset);

	while(1)
	{
		rset = allset;
		rdy = select(maxfd + 1, &rset, NULL, NULL, NULL);
		if(FD_ISSET(lcfd, &rset))
		{
			addr_len = sizeof(cin);
			if((cfd=accept(lcfd,(struct sockaddr *)(&cin),&addr_len))==-1)
			{
				fprintf(stderr,"Accept error:%s\n\a",strerror(errno));
				return;
			}
			printf("New client connection, fd=%d\n", cfd);

			for(i = 0; i<FD_SETSIZE; i++)
			{
				if(client[i] <= 0)
				{
					client[i] = cfd;
					break;
				}
			}

			if(i == FD_SETSIZE)
			{
				printf("too many clients\n");
				return;
			}

			FD_SET(cfd, &allset);

			if(cfd > maxfd)
			{
				maxfd = cfd;
			}

			if(i > maxi)
			{
				maxi = i;
			}

			if(--rdy <= 0)
			{
				continue;
			}
		}

		if(FD_ISSET(lgfd, &rset))
		{
			addr_len = sizeof(cin);
			if((cfd=accept(lgfd,(struct sockaddr *)(&cin),&addr_len))==-1)
			{
				fprintf(stderr,"Accept error:%s\n\a",strerror(errno));
				return;
			}
			printf("New gateway connection, fd=%d\n", cfd);

			for(i = 0; i<FD_SETSIZE; i++)
			{
				if(gateway[i] <= 0)
				{
					gateway[i] = cfd;
					break;
				}
			}

			if(i == FD_SETSIZE)
			{
				printf("too many clients\n");
				return;
			}

			FD_SET(cfd, &allset);

			if(cfd > maxfd)
			{
				maxfd = cfd;
			}

			if(i > maxi)
			{
				maxi = i;
			}

			if(--rdy <= 0)
			{
				continue;
			}
		}

		for(i = 0;i< FD_SETSIZE;i++)
		{
			if((sfd = client[i]) < 0)
			{
				continue;
			}

			if(FD_ISSET(sfd, &rset))
			{
				n = read(sfd,buffer,MAX_LINE);
				if(n == 0)
				{
					printf("the other side has been closed. \n");
					fflush(stdout);
					close(sfd);
					FD_CLR(sfd, &allset);
					client[i] = -1;
				}
				else
				{
					int issend = 0;
					for(i = gw_fi; i<FD_SETSIZE; i++)
					{
						if(gateway[i] > 0)
						{
							write(gateway[i], buffer, n);
							issend = 1;
							break;
						}
					}

					if(!issend)
					{
						for(i = 0; i<gw_fi; i++)
						{
							if(gateway[i] > 0)
							{
								write(gateway[i], buffer, n);
								issend = 1;
								break;
							}
						}
					}

					if(issend)
					{
						if(i <FD_SETSIZE)
						{
							gw_fi = i;
						}
						else
						{
							gw_fi = 0;
						}
					}
				}

				if(--rdy <= 0)
				{
					break;
				}
			}
		}

		for(i = 0;i< FD_SETSIZE;i++)
		{
			if((sfd = gateway[i]) < 0)
			{
				continue;
			}

			if(FD_ISSET(sfd, &rset))
			{
				n = read(sfd,buffer,MAX_LINE);
				if(n == 0)
				{
					printf("the other side has been closed. \n");
					fflush(stdout);
					close(sfd);
					FD_CLR(sfd, &allset);
					gateway[i] = -1;
				}
				else
				{
					int issend = 0;
					for(i = cli_fi; i<FD_SETSIZE; i++)
					{
						if(client[i] > 0)
						{
							write(client[i], buffer, n);
							issend = 1;
							break;
						}
					}

					if(!issend)
					{
						for(i = 0; i<cli_fi; i++)
						{
							if(client[i] > 0)
							{
								write(client[i], buffer, n);
								issend = 1;
								break;
							}
						}
					}

					if(issend)
					{
						if(i <FD_SETSIZE)
						{
							cli_fi = i;
						}
						else
						{
							cli_fi = 0;
						}
					}
				}

				if(--rdy <= 0)
				{
					break;
				}
			}
		}
	}

	close(lcfd);
	close(lgfd);
}

