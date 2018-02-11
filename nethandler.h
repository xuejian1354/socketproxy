/*
 * nethandler.h
 */
#ifndef __NETHANDLER_H__
#define __NETHANDLER_H__

#include "globals.h"
#include "netlist.h"

struct timespec *get_timespec();

int net_tcp_connect();
int net_tcp_recv(int fd);

void time_handler();

#endif  // __NETHANDLER_H__

