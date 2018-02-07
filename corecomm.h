/*
 * corecomm.h
 */
#ifndef __CORECOMM_H__
#define __CORECOMM_H__

#include "globals.h"
#include "nethandler.h"

int select_init();
void select_set(int fd);
void select_wtset(int fd);
void select_clr(int fd);
void select_wtclr(int fd);
int select_listen();

#endif  //__CORECOMM_H__