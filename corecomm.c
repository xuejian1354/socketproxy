/*
 * corecomm.c
 */
#include "corecomm.h"
#include <signal.h>

static int pt_index = 0;
static pthread_mutex_t mutex;
static pthread_t ptid[PTHREAD_SELECT_NUM];
static int before_fd = -1;
static int maxfd[PTHREAD_SELECT_NUM];
static fd_set global_rdfs[PTHREAD_SELECT_NUM];
static fd_set global_wtfs[PTHREAD_SELECT_NUM];
static sigset_t sigmask;

int get_query_index()
{
	return pt_index;
}

int get_query_indexpp()
{
	int ret = pt_index;
	if(pt_index < PTHREAD_SELECT_NUM-1)
	{
		pt_index++;
	}
	else
	{
		pt_index = 0;
	}

	return ret;
}

int select_init()
{
	int i;
	for(i=0; i<PTHREAD_SELECT_NUM; i++)
	{
		maxfd[i] = 0;
		FD_ZERO(&global_rdfs[i]);
		FD_ZERO(&global_wtfs[i]);
	}

	if(sigemptyset(&sigmask) < 0)
    {
		perror("sigemptyset");
		return -1;
    }

    if(sigaddset(&sigmask, SIGALRM) < 0)
    {
		perror("sigaddset");
		return -1;
    }

	return 0;
}

int select_set_with_index(int index, int fd)
{
	FD_SET(fd, &global_rdfs[index]);
	maxfd[index] = maxfd[index]>fd?maxfd[index]:fd+1;
	return index;
}

int select_set(int fd)
{
	int i = get_query_indexpp();
	return select_set_with_index(i, fd);
}

int select_wtset_with_index(int index, int fd)
{
	FD_SET(fd, &global_wtfs[index]);
	maxfd[index] = maxfd[index]>fd?maxfd[index]:fd+1;
	return index;
}

int select_wtset(int fd)
{
	int i = get_query_indexpp();
	return select_wtset_with_index(i, fd);
}

void select_clr(int index, int fd)
{
	FD_CLR(fd, &global_rdfs[index]);
}

void select_wtclr(int index, int fd)
{
	FD_CLR(fd, &global_wtfs[index]);
}

int select_listen(int index)
{
	int i, ret;

	fd_set current_rdfs = global_rdfs[index];
	fd_set current_wtfs = global_wtfs[index];
	ret = pselect(maxfd[index], &current_rdfs, &current_wtfs, NULL, get_timespec(), &sigmask);
	if(ret > 0)
	{
		for(i=0; i<maxfd[index]; i++)
		{
			if(before_fd != i)
			{
fd_is_set:
				if(FD_ISSET(i, &current_rdfs) || FD_ISSET(i, &current_wtfs))
				{
					pthread_mutex_lock(&mutex);
					before_fd = i;
					ret = net_tcp_recv(i);
					pthread_mutex_unlock(&mutex);
					return ret;
				}
				else if(before_fd == i)
				{
					break;
				}
			}
		}

		if(before_fd >= 0)
		{
			i = before_fd;
			goto fd_is_set;
		}
	}
	else if (ret == 0)
	{
		time_handler(index);
	}
	else
	{
		perror("pselect");
		return -1;
	}

	return 0;
}

void *select_excute(void *arg)
{
	int index = (int)arg;
	while(get_end())
	{
		if(select_listen(index) < 0)
		{
			//usleep(1000);
			set_end(0);
		}
		usleep(1000);
	}
	//pthread_exit(NULL);
}

int pthread_with_select()
{
	int i = 1, ret;
	pthread_mutex_init(&mutex, NULL);

	while(i < PTHREAD_SELECT_NUM)
	{
		ret = pthread_create(&ptid[i], NULL, select_excute, (void *)i);
		if(ret)
		{
			pthread_mutex_destroy(&mutex);
			perror("pthread_select()");
			return -1;
		}
		i++;
	}

	select_excute(0);
	pthread_mutex_destroy(&mutex);
	return 0;
}

