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

void main(int argc, char **argv) {
	int lfd;
	int cfd;
	int sfd;
	int rdy;

	struct sockaddr_in sin;
	struct sockaddr_in cin;

	int client[FD_SETSIZE];

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

	int listen_port = 8280;
	if (argc > 1) {
		listen_port = atoi(argv[1]);
	}

	bzero(&sin, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(listen_port);

	if ((lfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Socket error:%s\n\a", strerror(errno));
		return;
	}

	setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if (bind(lfd, (struct sockaddr *) (&sin), sizeof(struct sockaddr)) == -1) {
		fprintf(stderr, "Bind error:%s\n\a", strerror(errno));
		return;
	}

	if (listen(lfd, 20) == -1) {
		fprintf(stderr, "Listen error:%s\n\a", strerror(errno));
		return;
	}

	printf("Accepting connections, listen %d .......\n", listen_port);

	maxfd = lfd;
	maxi = -1;

	for (i = 0; i < FD_SETSIZE; i++) {
		client[i] = -1;
	}

	FD_ZERO(&allset);
	FD_SET(lfd,&allset);

	while (1) {
		rset = allset;
		rdy = select(maxfd + 1, &rset, NULL, NULL, NULL);
		if (FD_ISSET(lfd, &rset)) {
			addr_len = sizeof(sin);
			if ((cfd = accept(lfd, (struct sockaddr *) (&cin), &addr_len))
					== -1) {
				fprintf(stderr, "Accept error:%s\n\a", strerror(errno));
				return;
			}
			printf("New connection, fd=%d\n", cfd);

			for (i = 0; i < FD_SETSIZE; i++) {
				if (client[i] <= 0) {
					client[i] = cfd;
					break;
				}
			}

			if (i == FD_SETSIZE) {
				printf("too many clients\n");
				return;
			}

			FD_SET(cfd, &allset);

			if (cfd > maxfd) {
				maxfd = cfd;
			}

			if (i > maxi) {
				maxi = i;
			}

			if (--rdy <= 0) {
				continue;
			}
		}

		for (i = 0; i < FD_SETSIZE; i++) {
			if ((sfd = client[i]) < 0) {
				continue;
			}

			if (FD_ISSET(sfd, &rset)) {
				n = read(sfd, buffer, MAX_LINE);
				if (n == 0) {
					printf("the other side has been closed. \n");
					fflush(stdout);
					close(sfd);
					FD_CLR(sfd, &allset);
					client[i] = -1;
				} else {
					inet_ntop(AF_INET, &cin.sin_addr, addr_p, sizeof(addr_p));
					addr_p[strlen(addr_p)] = '\0';

					printf("Client Ip is %s, port is %d\n", addr_p,
							ntohs(cin.sin_port));

					write(sfd, buffer, n);
				}

				if (--rdy <= 0) {
					break;
				}
			}
		}
	}

	close(lfd);
}

