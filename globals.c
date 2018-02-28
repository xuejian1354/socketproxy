/*
 * globals.c
 */
#include "globals.h"
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <sys/socket.h>  
#include <sys/ioctl.h> 
#include <netinet/if_ether.h>  
#include <net/if.h>
#include <linux/sockios.h>


#ifdef __cplusplus
extern "C" {
#endif

static char current_time[64];
static int end_flag = 1;
static int test = 0;
static int daemon_run = 0;

static char host_addr[64];
static int host_port;

#ifdef GWLINK_WITH_SOCKS5_PASS
char auth_user[128] = DEFAULT_USER;
char auth_pass[128] = DEFAULT_PASS;
#endif

static int transport = DEFAULT_TRANSPORT;

static char macdev[64] = DEFAULT_MACDEV;
static int maxconn = SERVER_TCPLINK_NUM;

static void end_handler(int sig);

int istest()
{
	return test;
}

int isdaemon()
{
	return daemon_run;
}

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

#ifdef GWLINK_WITH_SOCKS5_PASS
char *get_auth_user()
{
	return auth_user;
}

char *get_auth_pass()
{
	return auth_pass;
}
#endif

int get_transport()
{
	return transport;
}

int get_max_connections_num()
{
	return maxconn;
}

int get_host_port_with_mac(int min, int max)
{
	struct ifreq ifreq;
	int sock;

	if((sock=socket(AF_INET,SOCK_STREAM,0))<0)
	{
		return 0;
	}

	strcpy(ifreq.ifr_name, macdev);

	if(ioctl(sock, SIOCGIFHWADDR, &ifreq) < 0)
	{
		return 0;
	}

	long macval = 0;
	macval = (macval<<8) + ifreq.ifr_hwaddr.sa_data[0];
	macval = (macval<<8) + ifreq.ifr_hwaddr.sa_data[1];
	macval = (macval<<8) + ifreq.ifr_hwaddr.sa_data[2];
	macval = (macval<<8) + ifreq.ifr_hwaddr.sa_data[3];
	macval = (macval<<8) + ifreq.ifr_hwaddr.sa_data[4];
	macval = (macval<<8) + ifreq.ifr_hwaddr.sa_data[5];
	return (macval % (max-min+1)) + min;
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
			return;
		}
		i++;
	}

	memset(host_addr, 0, sizeof(host_addr));
	strcpy(host_addr, addrstr);
	host_port = 0;
}

int start_params(int argc, char **argv)
{
	int ch;
	int isget = 0;
	opterr = 0;

	const char *optstrs = "a:c:p:e:m:d:t:h";
    while((ch = getopt(argc, argv, optstrs)) != -1)
    {
		switch(ch)
		{
		case 'h':
#ifdef GWLINK_WITH_SOCKS5_PASS
			AI_PRINTF("Usage: %s [-t print|daemon] -c <host[:port]> [-p transport] [-a user:pass] [-e macdev] [-m maxconn]\n", argv[0]);
#else
			AI_PRINTF("Usage: %s [-t print|daemon] -c <host[:port]> [-p transport] [-e macdev] [-m maxconn]\n", argv[0]);
#endif
			AI_PRINTF("Default\n");
			AI_PRINTF("    port        (macdev dependence)\n");
#ifdef GWLINK_WITH_SOCKS5_PASS
			AI_PRINTF("    user:pass   %s:%s\n", DEFAULT_USER, DEFAULT_PASS);
#endif
			AI_PRINTF("    transport   %d\n", DEFAULT_TRANSPORT);
			AI_PRINTF("    macdev      %s\n", DEFAULT_MACDEV);
			AI_PRINTF("    maxconn     %d\n", SERVER_TCPLINK_NUM);
			return 1;

		case 't':
			isget = 1;
			if(!strcmp(optarg, "print"))
			{
				test = 1;
			}
			else if(!strcmp(optarg, "daemon"))
			{
				daemon_run = 1;
				if(fork() > 0) {
					usleep(1000);
					return 1;
				}
				setsid();
			}
			break;

		case 'c':
			isget = 1;
			split_host_and_port(optarg);
			break;

		case 'p':
			isget = 1;
			transport = atoi(optarg);
			break;
#ifdef GWLINK_WITH_SOCKS5_PASS
		case 'a':
			isget = 1;
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
		case 'e':
			isget = 1;
			memset(macdev, 0, sizeof(macdev));
			strcpy(macdev, optarg);
			break;

		case 'm':
			isget = 1;
			maxconn = atoi(optarg);
			break;
		}
	}

	if(!isget)
	{
		AI_PRINTF("Unrecognize arguments.\n");
		AI_PRINTF("\'%s -h\' get more help infomations.\n", argv[0]);
		return -1;
	}

	if(!host_port)
	{
		if((host_port = get_host_port_with_mac(40000, 50000)) == 0)
		{
			AI_PRINTF("Can't get mac addr from %s\n", macdev);
			AI_PRINTF("\'%s -h\' get more help infomations.\n", argv[0]);
			return -1;
		}
	}

	return 0;
}

void process_signal_register()
{
	 signal(SIGINT, end_handler);
	 signal(SIGTSTP, end_handler);
	 signal(SIGPIPE, SIG_IGN);	//send error
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
#ifdef DLOG_PRINT	
	system("rm -f "TMP_LOG);
#endif
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

unsigned long get_system_time()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec*1000*1000 + tv.tv_usec;
}

#ifdef __cplusplus
}
#endif

