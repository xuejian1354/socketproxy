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

static char host[64];
static int port;

static void end_handler(int sig);

int get_end()
{
	return end_flag;
}

char *get_host()
{
	return host;
}

int get_port()
{
	return port;
}

void split_host_and_port(char *addrstr)
{
	int i = 0;
	int addrlen = strlen(addrstr);

	while (i < addrlen)
	{
		if(*(addrstr+i) == ':')
		{
			memset(host, 0, sizeof(host));
			strncpy(host, addrstr, i);
			port = atoi(addrstr+i+1);
			break;
		}
		i++;
	}
}

int start_params(int argc, char **argv)
{
	int ch;
	opterr = 0; 

	const char *optstrs = "m:h";
    while((ch = getopt(argc, argv, optstrs)) != -1)
    {
		switch(ch)
		{
		case 'h':
			AI_PRINTF("Usage: %s -m <host:port>\n", argv[0]);
			return 1;

		case 'm':
			split_host_and_port(optarg);
			return 0;
		}
	}

	AI_PRINTF("Unrecognize arguments.\n");
	AI_PRINTF("\'%s -h\' get more help infomations.\n", argv[0]);

	return -1;
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

