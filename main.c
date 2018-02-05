/*
 * main.c
 *
 * Sam Chen <xuejian1354@163.com>
 *
 */
#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif

int main(int argc, char **argv)
{
	if(start_params(argc, argv) != 0)
	{
		return 1;
	}

	process_signal_register();

	if(mach_init() < 0)
	{
		return 1;
	}

	AI_PRINTF("[%s] %s start, getpid=%d\n", get_current_time(), TARGET_NAME, getpid());
#ifndef DLOG_PRINT
	AO_PRINTF("[%s] %s start, getpid=%d\n", get_current_time(), TARGET_NAME, getpid());
#endif

	if (select_init() < 0)
	{
		return 1;
	}

	if (socket_tcp_client_connect(get_host(), get_port()) < 0)
	{
		return 1;
	}

	while(get_end())
	{
		if(select_listen() < 0)
		{
			usleep(1000);
		}
		usleep(100);
	}

	AI_PRINTF("[%s] %d exit\n", get_current_time(), getpid());
#ifndef DLOG_PRINT
	AO_PRINTF("[%s] %d exit\n", get_current_time(), getpid());
#endif
	return 0;
}

#ifdef __cplusplus
}
#endif
