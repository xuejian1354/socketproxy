/*
 * corecomm.c
 */
#include "corecomm.h"
#include <signal.h>

static int maxfd;
static fd_set global_rdfs;
static fd_set global_wtfs;
static sigset_t sigmask;

int select_init()
{
	maxfd = 0;
	FD_ZERO(&global_rdfs);
	FD_ZERO(&global_wtfs);

	if(sigemptyset(&sigmask) < 0)
    {
		perror("sigemptyset");
		return -1;
    }

    if(sigaddset(&sigmask,SIGALRM) < 0)
    {
		perror("sigaddset");
		return -1;
    }

	return 0;
}

void select_set(int fd)
{
	FD_SET(fd, &global_rdfs);
	maxfd = maxfd>fd?maxfd:fd+1;
}

void select_wtset(int fd)
{
	FD_SET(fd, &global_wtfs);
	maxfd = maxfd>fd?maxfd:fd+1;
}

void select_clr(int fd)
{
	FD_CLR(fd, &global_rdfs);
}

void select_wtclr(int fd)
{
	FD_CLR(fd, &global_wtfs);
}

int select_listen()
{
	int i, ret;

	fd_set current_rdfs = global_rdfs;
	fd_set current_wtfs = global_wtfs;
	ret = pselect(maxfd, &current_rdfs, &current_wtfs, NULL, get_timespec(), &sigmask);
	if(ret > 0)
	{
		for(i=0; i<maxfd; i++)
		{
			if(FD_ISSET(i, &current_rdfs) || FD_ISSET(i, &current_wtfs))
			{
				return net_tcp_recv(i);
			}
		}
	}
	else if (ret == 0)
	{
		time_handler();
	}
	else
	{
		perror("pselect");
		return -1;
	}

	return 0;
}

