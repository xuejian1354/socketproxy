/*
 * main.c
 */
#include <globals.h>

#ifdef __cplusplus
extern "C" {
#endif

int main(int argc, char **argv) {
	if (start_params(argc, argv) != 0) {
		return 1;
	}

	process_signal_register();

	if (mach_init() < 0) {
		return 1;
	}

	AI_PRINTF("[%s] %s start, getpid=%d\n",
			get_current_time(), TARGET_NAME, getpid());

	if (select_init() < 0) {
		return 1;
	}

	if (net_tcp_connect() < 0) {
		return 1;
	}

	pthread_with_select();

	AI_PRINTF("[%s] %s exit, %d\n", get_current_time(), TARGET_NAME, getpid());
	return 0;
}

#ifdef __cplusplus
}
#endif
