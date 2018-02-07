/*
 * globals.h
 */
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include "mtypes.h"
#include "dlog.h"
#include <time.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAXSIZE	8192
#define DEFAULT_TRANSPORT	1080
#define SERVER_TCPLINK_NUM	200
#define DEFAULT_MACDEV	"eth0"

int istest();

void set_end(int end);
int get_end();
char *get_host_addr();
int get_host_port();
int get_transport();
int get_max_connections_num();

int start_params(int argc, char **argv);

int mach_init();

void process_signal_register();

char *get_current_time();
char *get_system_time();

#ifdef __cplusplus
}
#endif

#endif	//__GLOBALS_H__
