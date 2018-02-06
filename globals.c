/* globals.c
 *
 * Sam Chen <xuejian1354@163.com>
 *
 */
#include "globals.h"
#include <errno.h>
#include <dirent.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

static char current_time[64];
static int end_flag = 1;

static char host_addr[64];
static int host_port;
static int transport = DEFAULT_TRANSPORT;

static void end_handler(int sig);

void set_end(int end)
{
	end_flag = end;
}


int get_end()
{
	return end_flag;
}

char *get_host_addr()
{
	return host_addr;
}

int get_host_port()
{
	return host_port;
}

int get_transport()
{
	return transport;
}


void split_host_and_port(char *addrstr)
{
	int i = 0;
	int addrlen = strlen(addrstr);

	while (i < addrlen)
	{
		if(*(addrstr+i) == ':')
		{
			memset(host_addr, 0, sizeof(host_addr));
			strncpy(host_addr, addrstr, i);
			host_port = atoi(addrstr+i+1);
			break;
		}
		i++;
	}
}

int start_params(int argc, char **argv)
{
	int ch;
	int isget = 0;
	opterr = 0; 

	const char *optstrs = "m:p:h";
    while((ch = getopt(argc, argv, optstrs)) != -1)
    {
		switch(ch)
		{
		case 'h':
			AI_PRINTF("Usage: %s -m <host:port> [-p transport]\n", argv[0]);
			AI_PRINTF("  Default transport is %d\n", DEFAULT_TRANSPORT);
			return 1;

		case 'm':
			isget = 1;
			split_host_and_port(optarg);
			break;

		case 'p':
			isget = 1;
			transport = atoi(optarg);
			break;
		}
	}

	if(!isget)
	{
		AI_PRINTF("Unrecognize arguments.\n");
		AI_PRINTF("\'%s -h\' get more help infomations.\n", argv[0]);
		return -1;
	}

	return 0;
}

void process_signal_register()
{
	 signal(SIGINT, end_handler);
	 signal(SIGTSTP, end_handler);
}

void end_handler(int sig)
{
	switch(sig)
	{
	case SIGINT:
		AI_PRINTF(" SIGINT\t");
		break;

	case SIGTSTP:
		AI_PRINTF(" SIGTSTP\t");
		break;
	}

	AI_PRINTF("\n");
	end_flag = 0;
}

int mach_init()
{
	system("rm -f "TMP_LOG);
	return 0;
}

char *get_current_time()
{
	time_t t;
	time(&t);
	bzero(current_time, sizeof(current_time));
	struct tm *tp= localtime(&t);
	strftime(current_time, 100, "%Y-%m-%d %H:%M:%S", tp); 

	return current_time;
}

char *get_system_time()
{
	struct timeval time;
	gettimeofday(&time, NULL);
	bzero(current_time, sizeof(current_time));
	sprintf(current_time, "%u", (uint32)time.tv_sec);

	return current_time;
}

#ifdef __cplusplus
}
#endif

