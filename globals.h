/* globals.h
 *
 * Sam Chen <xuejian1354@163.com>
 *
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

void set_end(int end);
int get_end();
char *get_host();
int get_port();


int start_params(int argc, char **argv);

int mach_init();

void process_signal_register();

char *get_current_time();
char *get_system_time();

#ifdef __cplusplus
}
#endif

#endif	//__GLOBALS_H__
